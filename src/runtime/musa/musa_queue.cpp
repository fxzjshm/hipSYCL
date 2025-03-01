/*
 * This file is part of hipSYCL, a SYCL implementation based on CUDA/HIP
 *
 * Copyright (c) 2019-2020 Aksel Alpay and contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "driver_types.h"
#include "hipSYCL/common/hcf_container.hpp"
#include "hipSYCL/glue/kernel_configuration.hpp"
#include "hipSYCL/runtime/adaptivity_engine.hpp"
#include "hipSYCL/runtime/application.hpp"
#include "hipSYCL/runtime/code_object_invoker.hpp"
#include "hipSYCL/runtime/musa/musa_instrumentation.hpp"
#include "hipSYCL/runtime/error.hpp"
#include "hipSYCL/runtime/util.hpp"
#include "hipSYCL/runtime/musa/musa_queue.hpp"
#include "hipSYCL/runtime/musa/musa_backend.hpp"
#include "hipSYCL/runtime/musa/musa_event.hpp"
#include "hipSYCL/runtime/musa/musa_device_manager.hpp"
#include "hipSYCL/runtime/musa/musa_code_object.hpp"
#include "hipSYCL/runtime/event.hpp"
#include "hipSYCL/runtime/hints.hpp"
#include "hipSYCL/runtime/inorder_queue.hpp"
#include "hipSYCL/runtime/kernel_launcher.hpp"
#include "hipSYCL/runtime/operations.hpp"
#include "hipSYCL/runtime/serialization/serialization.hpp"
#include "hipSYCL/runtime/util.hpp"
#include "hipSYCL/runtime/queue_completion_event.hpp"

#ifdef HIPSYCL_WITH_SSCP_COMPILER

#include "hipSYCL/compiler/llvm-to-backend/musa/LLVMToMusaFactory.hpp"
#include "hipSYCL/glue/llvm-sscp/jit.hpp"

#endif

#include <musa_runtime_api.h>
#include <musa_runtime.h> //for make_cudaPitchedPtr
#include <musa.h> // For kernels launched from modules

#include <cassert>
#include <memory>

namespace hipsycl {
namespace rt {

namespace {

void host_synchronization_callback(musaStream_t stream, musaError_t status,
                                   void *userData) {
  
  assert(userData);
  dag_node_ptr* node = static_cast<dag_node_ptr*>(userData);
  
  if(status != musaSuccess) {
    register_error(__hipsycl_here(),
                   error_info{"musa_queue callback: MUSA returned error code.",
                              error_code{"MUSA", status}});
  }
  else {
    (*node)->wait();
  }
  delete node;
}


class musa_instrumentation_guard {
public:
  musa_instrumentation_guard(musa_queue *q,
                             operation &op, dag_node_ptr node) 
                             : _queue{q}, _operation{&op}, _node{node} {
    assert(q);
    
    if(!_node)
      return;

    if (_node->get_execution_hints()
            .has_hint<
                rt::hints::request_instrumentation_submission_timestamp>()) {

      op.get_instrumentations()
          .add_instrumentation<instrumentations::submission_timestamp>(
            std::make_shared<musa_submission_timestamp>(profiler_clock::now()));
    }

    if (_node->get_execution_hints().has_hint<
                rt::hints::request_instrumentation_start_timestamp>()) {

      _task_start = _queue->insert_event();

      op.get_instrumentations()
          .add_instrumentation<instrumentations::execution_start_timestamp>(
              std::make_shared<musa_execution_start_timestamp>(
                  _queue->get_timing_reference(), _task_start));
    }
  }

  ~musa_instrumentation_guard() {
    if(!_node)
      return;
    
    if (_node->get_execution_hints()
            .has_hint<rt::hints::request_instrumentation_finish_timestamp>()) {
      std::shared_ptr<dag_node_event> task_finish = _queue->insert_event();

      if(_task_start) {
        _operation->get_instrumentations()
            .add_instrumentation<instrumentations::execution_finish_timestamp>(
                std::make_shared<musa_execution_finish_timestamp>(
                    _queue->get_timing_reference(), _task_start, task_finish));
      } else {
        _operation->get_instrumentations()
            .add_instrumentation<instrumentations::execution_finish_timestamp>(
                std::make_shared<musa_execution_finish_timestamp>(
                    _queue->get_timing_reference(), task_finish));
      }
    }
  }

private:
  musa_queue* _queue;
  operation* _operation;
  dag_node_ptr _node;
  std::shared_ptr<dag_node_event> _task_start;
};

result launch_kernel_from_module(MUmodule module,
                                 const std::string &kernel_name,
                                 const rt::range<3> &grid_size,
                                 const rt::range<3> &block_size,
                                 unsigned shared_memory, musaStream_t stream,
                                 void **kernel_args) {
  MUfunction f;
  MUresult err = muModuleGetFunction(&f, module, kernel_name.c_str());

  if (err != MUSA_SUCCESS) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: could not extract kernel from module",
                                 error_code{"MU", static_cast<int>(err)}});
  }

  err = muLaunchKernel(f, static_cast<unsigned>(grid_size.get(0)),
                       static_cast<unsigned>(grid_size.get(1)),
                       static_cast<unsigned>(grid_size.get(2)),
                       static_cast<unsigned>(block_size.get(0)),
                       static_cast<unsigned>(block_size.get(1)),
                       static_cast<unsigned>(block_size.get(2)),
                       shared_memory, stream, kernel_args, nullptr);

  if (err != MUSA_SUCCESS) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: could not submit kernel from module",
                                 error_code{"MU", static_cast<int>(err)}});
  }
  
  return make_success();
}
}


void musa_queue::activate_device() const {
  musa_device_manager::get().activate_device(_dev.get_id());
}

musa_queue::musa_queue(musa_backend *be, device_id dev, int priority)
    : _dev{dev}, _stream{nullptr},
      _multipass_code_object_invoker{this},
      _sscp_code_object_invoker{this}, _backend{be},
      _kernel_cache{kernel_cache::get()} {
  this->activate_device();

  musaError_t err;
  if(priority == 0) {
    err = musaStreamCreateWithFlags(&_stream, musaStreamNonBlocking);
  } else {
    // TODO Clamp priority to priority range allowed by CUDA.
    err = musaStreamCreateWithPriority(&_stream, musaStreamNonBlocking, priority);
  }
  if (err != musaSuccess) {
    register_error(__hipsycl_here(),
                   error_info{"musa_queue: Couldn't construct backend stream",
                              error_code{"MUSA", err}});
    return;
  }

  _reference_event = host_timestamped_event{this};
}

MUstream_st* musa_queue::get_stream() const { return _stream; }

musa_queue::~musa_queue() {
  auto err = musaStreamDestroy(_stream);
  if (err != musaSuccess) {
    register_error(__hipsycl_here(),
                   error_info{"musa_queue: Couldn't destroy stream",
                              error_code{"MUSA", err}});
  }
}

/// Inserts an event into the stream
std::shared_ptr<dag_node_event> musa_queue::insert_event() {
  musaEvent_t evt;
  auto event_creation_result =
      _backend->get_event_pool(_dev)->obtain_event(evt);
  if(!event_creation_result.is_success()) {
    register_error(event_creation_result);
    return nullptr;
  }

  musaError_t err = musaEventRecord(evt, this->get_stream());
  if (err != musaSuccess) {
    register_error(
        __hipsycl_here(),
        error_info{"musa_queue: Couldn't record event", error_code{"MUSA", err}});
    return nullptr;
  }

  return std::make_shared<musa_node_event>(_dev, evt,
                                           _backend->get_event_pool(_dev));
}

std::shared_ptr<dag_node_event> musa_queue::create_queue_completion_event() {
  return std::make_shared<queue_completion_event<musaEvent_t, musa_node_event>>(
      this);
}


result musa_queue::submit_memcpy(memcpy_operation & op, dag_node_ptr node) {

  device_id source_dev = op.source().get_device();
  device_id dest_dev = op.dest().get_device();

  assert(op.source().get_access_ptr());
  assert(op.dest().get_access_ptr());

  musaMemcpyKind copy_kind = musaMemcpyHostToDevice;

  if (source_dev.get_full_backend_descriptor().sw_platform == api_platform::musa) {
    if (dest_dev.get_full_backend_descriptor().sw_platform ==
        api_platform::musa) {
      assert(source_dev.get_full_backend_descriptor().hw_platform ==
                 dest_dev.get_full_backend_descriptor().hw_platform &&
             "Attempted to execute explicit device<->device copy operation "
             "between devices from different MUSA hardware backends");
      copy_kind = musaMemcpyDeviceToDevice;
    } else if (dest_dev.get_full_backend_descriptor().hw_platform ==
               hardware_platform::cpu) {
      copy_kind = musaMemcpyDeviceToHost;
    } else
      assert(false && "Unknown copy destination platform");
  } else if (source_dev.get_full_backend_descriptor().hw_platform ==
             hardware_platform::cpu) {
    if (dest_dev.get_full_backend_descriptor().sw_platform ==
        api_platform::musa) {
      copy_kind = musaMemcpyHostToDevice;
    } else
      assert(false && "Unknown copy destination platform");
  } else
    assert(false && "Unknown copy source platform");


  range<3> transfer_range = op.get_num_transferred_elements();

  int dimension = 0;
  if (transfer_range[0] > 1)
    dimension = 3;
  else if (transfer_range[1] > 1)
    dimension = 2;
  else
    dimension = 1;

  // If we transfer the entire buffer, treat it as 1D memcpy for performance.
  // TODO: The same optimization could also be applied for the general case
  // when regions are contiguous
  if (op.get_num_transferred_elements() == op.source().get_allocation_shape() &&
      op.get_num_transferred_elements() == op.dest().get_allocation_shape() &&
      op.source().get_access_offset() == id<3>{} &&
      op.dest().get_access_offset() == id<3>{})
    dimension = 1;

  assert(dimension >= 1 && dimension <= 3);


  musa_instrumentation_guard instrumentation{this, op, node};

  musaError_t err = musaSuccess;
  if (dimension == 1) {
    err = musaMemcpyAsync(
        op.dest().get_access_ptr(), op.source().get_access_ptr(),
        op.get_num_transferred_bytes(), copy_kind, get_stream());
    
  } else if (dimension == 2) {
    err = musaMemcpy2DAsync(
        op.dest().get_access_ptr(),
        extract_from_range3<2>(op.dest().get_allocation_shape())[1] *
            op.dest().get_element_size(),
        op.source().get_access_ptr(),
        extract_from_range3<2>(op.source().get_allocation_shape())[1] *
            op.source().get_element_size(),
        extract_from_range3<2>(op.get_num_transferred_elements())[1] *
            op.source().get_element_size(),
        extract_from_range3<2>(op.get_num_transferred_elements())[0], copy_kind,
        get_stream());
    
  } else {
    
    musaMemcpy3DParms params {};
    params.srcPtr = make_musaPitchedPtr(op.source().get_access_ptr(),
                                        op.source().get_allocation_shape()[2] *
                                            op.source().get_element_size(),
                                        op.source().get_allocation_shape()[2],
                                        op.source().get_allocation_shape()[1]);
    params.dstPtr = make_musaPitchedPtr(op.dest().get_access_ptr(),
                                        op.dest().get_allocation_shape()[2] *
                                            op.dest().get_element_size(),
                                        op.dest().get_allocation_shape()[2],
                                        op.dest().get_allocation_shape()[1]);
    params.extent = {op.get_num_transferred_elements()[2] *
                        op.source().get_element_size(),
                    op.get_num_transferred_elements()[1],
                    op.get_num_transferred_elements()[0]};
    params.kind = copy_kind;

    err = musaMemcpy3DAsync(&params, get_stream());
  }

  if (err != musaSuccess) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: Couldn't submit memcpy",
                                  error_code{"MUSA", err}});
  }
  
  return make_success();
}

result musa_queue::submit_kernel(kernel_operation &op, dag_node_ptr node) {

  this->activate_device();
  rt::backend_kernel_launcher *l =
      op.get_launcher().find_launcher(backend_id::musa);
  if (!l)
    return make_error(__hipsycl_here(), error_info{"Could not obtain backend kernel launcher"});
  l->set_params(this);

  rt::backend_kernel_launch_capabilities cap;
  cap.provide_multipass_invoker(&_multipass_code_object_invoker);
  cap.provide_sscp_invoker(&_sscp_code_object_invoker);
  l->set_backend_capabilities(cap);
  
  musa_instrumentation_guard instrumentation{this, op, node};
  l->invoke(node.get(), op.get_launcher().get_kernel_configuration());

  return make_success();
}

result musa_queue::submit_prefetch(prefetch_operation& op, dag_node_ptr node) {
#ifndef _WIN32
  
  musaError_t err = musaSuccess;
  
  musa_instrumentation_guard instrumentation{this, op, node};
  if (op.get_target().is_host()) {
    err = musaMemPrefetchAsync(op.get_pointer(), op.get_num_bytes(),
                                        musaCpuDeviceId, get_stream());
  } else {
    err = musaMemPrefetchAsync(op.get_pointer(), op.get_num_bytes(),
                                        _dev.get_id(), get_stream());
  }


  if (err != musaSuccess) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: musaMemPrefetchAsync() failed",
                                 error_code{"MUSA", err}});
  }
#else
  HIPSYCL_DEBUG_WARNING << "musa_queue: Ignoring prefetch() hint"
                        << std::endl;
#endif // _WIN32
  return make_success();
}

result musa_queue::submit_memset(memset_operation &op, dag_node_ptr node) {

  musa_instrumentation_guard instrumentation{this, op, node};
  
  musaError_t err = musaMemsetAsync(op.get_pointer(), op.get_pattern(),
                                    op.get_num_bytes(), get_stream());
  

  if (err != musaSuccess) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: musaMemsetAsync() failed",
                                 error_code{"MUSA", err}});
  }

  return make_success();
}

/// Causes the queue to wait until an event on another queue has occured.
/// the other queue must be from the same backend
result musa_queue::submit_queue_wait_for(dag_node_ptr node) {
  auto evt = node->get_event();
  assert(dynamic_is<inorder_queue_event<musaEvent_t>>(evt.get()));

  inorder_queue_event<musaEvent_t> *musa_evt =
      cast<inorder_queue_event<musaEvent_t>>(evt.get());
  
  auto err = musaStreamWaitEvent(_stream, musa_evt->request_backend_event(), 0);
  if (err != musaSuccess) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: musaStreamWaitEvent() failed",
                                 error_code{"MUSA", err}});
  }

  return make_success();
}

result musa_queue::submit_external_wait_for(dag_node_ptr node) {

  dag_node_ptr* user_data = new dag_node_ptr;
  assert(user_data);
  *user_data = node;

  auto err = 
      musaStreamAddCallback(_stream, host_synchronization_callback,
                           reinterpret_cast<void *>(user_data), 0);

  if (err != musaSuccess) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: Couldn't submit stream callback",
                                 error_code{"MUSA", err}});
  }
  
  return make_success();
}

result musa_queue::wait() {

  auto err = musaStreamSynchronize(_stream);

  if(err != musaSuccess) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: Couldn't synchronize with stream",
                                 error_code{"MUSA", err}});
  }

  return make_success();
}

result musa_queue::submit_multipass_kernel_from_code_object(
    const kernel_operation &op, hcf_object_id hcf_object,
    const std::string &backend_kernel_name, const rt::range<3> &grid_size,
    const rt::range<3> &block_size, unsigned dynamic_shared_mem,
    void **kernel_args, std::size_t num_args) {

  this->activate_device();

  // For now we need to extract HCF in order to get a list of available
  // compilation targets (list of embedded device images in HCF).
  // TODO we could cache this vector to avoid retrieving HCF for every kernel launch
  const common::hcf_container *hcf =
        rt::hcf_cache::get().get_hcf(hcf_object);
  if (!hcf)
    return make_error(
        __hipsycl_here(),
        error_info{"musa_queue: Could not access requested HCF object"});

  assert(hcf->root_node());
  std::vector<std::string> available_targets = hcf->root_node()->get_subnodes();
  assert(!available_targets.empty());

  // TODO Select best compiled target based on actual device - currently
  // we just use the first device image no matter which device it was
  // compiled for
  std::string selected_target = available_targets[0];

  int device = _dev.get_id();

  glue::kernel_configuration config;
  config.append_base_configuration(
      glue::kernel_base_config_parameter::backend_id, backend_id::musa);
  config.append_base_configuration(
      glue::kernel_base_config_parameter::compilation_flow,
      compilation_flow::explicit_multipass);
  config.append_base_configuration(
      glue::kernel_base_config_parameter::hcf_object_id, hcf_object);
  config.append_base_configuration(
      glue::kernel_base_config_parameter::target_arch, selected_target);

  auto binary_configuration_id = config.generate_id();
  auto code_object_configuration_id = binary_configuration_id;
  glue::kernel_configuration::extend_hash(
      code_object_configuration_id,
      glue::kernel_base_config_parameter::runtime_device, device);

  // Will be invoked by the kernel cache in case there is a miss in the kernel
  // cache and we have to construct a new code object
  auto code_object_constructor = [&]() -> code_object* {
    const common::hcf_container::node *tn =
        hcf->root_node()->get_subnode(selected_target);
    if(!tn)
      return nullptr;
    if(!tn->has_binary_data_attached())
      return nullptr;
    
    std::string source_code;
    if(!hcf->get_binary_attachment(tn, source_code)) {
      HIPSYCL_DEBUG_ERROR << "musa_queue: Could not extract MUSA code from "
                              "HCF node; invalid HCF data?"
                          << std::endl;
      return nullptr;
    }

    musa_executable_object *exec_obj = new musa_multipass_executable_object{
        hcf_object, selected_target, source_code, device};
    result r = exec_obj->get_build_result();

    if(!r.is_success()) {
      register_error(r);
      delete exec_obj;
      return nullptr;
    }

    return exec_obj;
  };

  const code_object *obj = _kernel_cache->get_or_construct_code_object(
      code_object_configuration_id, code_object_constructor);

  if(!obj) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: Code object construction failed"});
  }

  MUmodule mumodule = static_cast<const musa_executable_object*>(obj)->get_module();
  assert(mumodule);

  // Need to find out full backend kernel name. This is necessary because
  // we don't know the *exact* kernel name until we know that we are in the clang 13+
  // name mangling path. It can be that we only have a fragment :(
  std::string full_kernel_name;
  for(const auto& name : obj->supported_backend_kernel_names()) {
    if(name.find(backend_kernel_name) != std::string::npos) {
      full_kernel_name = name;
      break;
    }
  }
  if(full_kernel_name.empty())
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: Could not discover full kernel "
                                 "name from partial backend kernel name"});

  return launch_kernel_from_module(mumodule, full_kernel_name, grid_size,
                                   block_size, dynamic_shared_mem, _stream,
                                   kernel_args);
}


result musa_queue::submit_sscp_kernel_from_code_object(
    const kernel_operation &op, hcf_object_id hcf_object,
    const std::string &kernel_name, const rt::range<3> &num_groups,
    const rt::range<3> &group_size, unsigned local_mem_size, void **args,
    std::size_t *arg_sizes, std::size_t num_args,
    const glue::kernel_configuration &initial_config) {
#ifdef HIPSYCL_WITH_SSCP_COMPILER

  this->activate_device();

  int device = this->_dev.get_id();

  musa_hardware_context *ctx = static_cast<musa_hardware_context *>(
      this->_backend->get_hardware_manager()->get_device(device));

  std::string target_arch_name = ctx->get_device_arch();
  unsigned compute_capability = ctx->get_compute_capability();

  const hcf_kernel_info *kernel_info =
      rt::hcf_cache::get().get_kernel_info(hcf_object, kernel_name);
  if(!kernel_info) {
    return make_error(
        __hipsycl_here(),
        error_info{"musa_queue: Could not obtain hcf kernel info for kernel " +
            kernel_name});
  }

  kernel_adaptivity_engine adaptivity_engine{
      hcf_object, kernel_name, kernel_info, num_groups,
      group_size, args,        arg_sizes,   num_args, local_mem_size};

  static thread_local glue::kernel_configuration config;
  config = initial_config;
  config.append_base_configuration(
      glue::kernel_base_config_parameter::backend_id, backend_id::musa);
  config.append_base_configuration(
      glue::kernel_base_config_parameter::compilation_flow,
      compilation_flow::sscp);
  config.append_base_configuration(
      glue::kernel_base_config_parameter::hcf_object_id, hcf_object);
  
  for(const auto& flag : kernel_info->get_compilation_flags())
    config.set_build_flag(flag);
  for(const auto& opt : kernel_info->get_compilation_options())
    config.set_build_option(opt.first, opt.second);

  config.set_build_option(glue::kernel_build_option::musa_target_device,
                          compute_capability);

  auto binary_configuration_id = adaptivity_engine.finalize_binary_configuration(config);
  auto code_object_configuration_id = binary_configuration_id;
  glue::kernel_configuration::extend_hash(
      code_object_configuration_id,
      glue::kernel_base_config_parameter::runtime_device, device);

  auto get_image_and_kernel_names =
      [&](std::vector<std::string> &contained_kernels) -> std::string {
    return adaptivity_engine.select_image_and_kernels(&contained_kernels);
  };

  auto jit_compiler = [&](std::string& compiled_image) -> bool {
    const common::hcf_container* hcf = rt::hcf_cache::get().get_hcf(hcf_object);
    
    std::vector<std::string> kernel_names;
    std::string selected_image_name = get_image_and_kernel_names(kernel_names);

    // Construct MUSA translator to compile the specified kernels
    std::unique_ptr<compiler::LLVMToBackendTranslator> translator = 
      compiler::createLLVMToMusaTranslator(kernel_names);

    // Lower kernels to MUSA
    auto err = glue::jit::compile(translator.get(),
        hcf, selected_image_name, config, compiled_image);
    
    if(!err.is_success()) {
      register_error(err);
      return false;
    }
    return true;
  };

  auto code_object_constructor = [&](const std::string& ptx_image) -> code_object* {

    std::vector<std::string> kernel_names;
    get_image_and_kernel_names(kernel_names);
    
    musa_sscp_executable_object *exec_obj = new musa_sscp_executable_object{
        ptx_image, target_arch_name, hcf_object, kernel_names, device, config};
    result r = exec_obj->get_build_result();

    HIPSYCL_DEBUG_INFO
        << "musa_queue: Successfully compiled SSCP kernels to module " << exec_obj->get_module()
        << std::endl;

    if(!r.is_success()) {
      register_error(r);
      delete exec_obj;
      return nullptr;
    }

    return exec_obj;
  };

  const code_object *obj = _kernel_cache->get_or_construct_jit_code_object(
      code_object_configuration_id, binary_configuration_id,
      jit_compiler, code_object_constructor);

  if(!obj) {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: Code object construction failed"});
  }

  MUmodule mumodule = static_cast<const musa_executable_object*>(obj)->get_module();
  assert(mumodule);

  glue::jit::cxx_argument_mapper arg_mapper{*kernel_info, args, arg_sizes,
                                            num_args};
  if(!arg_mapper.mapping_available()) {
    return make_error(
        __hipsycl_here(),
        error_info{
            "musa_queue: Could not map C++ arguments to kernel arguments"});
  }
  return launch_kernel_from_module(mumodule, kernel_name, num_groups,
                                   group_size, local_mem_size, _stream,
                                   arg_mapper.get_mapped_args());

#else
  return make_error(
      __hipsycl_here(),
      error_info{
          "musa_queue: SSCP kernel launch was requested, but hipSYCL was "
          "not built with MUSA SSCP support."});
#endif
}

device_id musa_queue::get_device() const {
  return _dev;
}

void *musa_queue::get_native_type() const {
  return static_cast<void*>(get_stream());
}

musa_multipass_code_object_invoker::musa_multipass_code_object_invoker(
    musa_queue *q)
    : _queue{q} {}

result musa_queue::query_status(inorder_queue_status &status) {
  auto err = musaStreamQuery(_stream);
  if(err == musaSuccess) {
    status = inorder_queue_status{true};
  } else if(err == musaErrorNotReady) {
    status = inorder_queue_status{false};
  } else {
    return make_error(__hipsycl_here(),
                      error_info{"musa_queue: Could not query stream status",
                                 error_code{"MU", static_cast<int>(err)}});
  }

  return make_success();
}

result musa_multipass_code_object_invoker::submit_kernel(
    const kernel_operation& op,
    hcf_object_id hcf_object,
    const rt::range<3> &num_groups,
    const rt::range<3> &group_size,
    unsigned local_mem_size, void **args,
    std::size_t *arg_sizes, std::size_t num_args,
    const std::string &kernel_name_tag,
    const std::string &kernel_body_name) {

  assert(_queue);

  std::string kernel_name = kernel_body_name;
  if(kernel_name_tag.find("__hipsycl_unnamed_kernel") == std::string::npos)
    kernel_name = kernel_name_tag;

  return _queue->submit_multipass_kernel_from_code_object(
      op, hcf_object, kernel_name, num_groups, group_size, local_mem_size, args,
      num_args);
}

result musa_sscp_code_object_invoker::submit_kernel(
    const kernel_operation &op, hcf_object_id hcf_object,
    const rt::range<3> &num_groups, const rt::range<3> &group_size,
    unsigned local_mem_size, void **args, std::size_t *arg_sizes,
    std::size_t num_args, const std::string &kernel_name,
    const glue::kernel_configuration &config) {

  return _queue->submit_sscp_kernel_from_code_object(
      op, hcf_object, kernel_name, num_groups, group_size, local_mem_size, args,
      arg_sizes, num_args, config);
}

}
}

