/*
 * This file is part of hipSYCL, a SYCL implementation based on CUDA/HIP
 *
 * Copyright (c) 2022 Aksel Alpay
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


#include "hipSYCL/runtime/musa/musa_event_pool.hpp"
#include "hipSYCL/runtime/musa/musa_device_manager.hpp"
#include "hipSYCL/runtime/error.hpp"
#include <musa_runtime_api.h>

namespace hipsycl {
namespace rt {

musa_event_factory::musa_event_factory(int device_id)
: _device_id{device_id} {}

result musa_event_factory::create(musaEvent_t& out) {
  musa_device_manager::get().activate_device(_device_id);

  musaEvent_t evt;
  auto err = musaEventCreate(&evt);
  if(err != musaSuccess) {
    return make_error(
        __hipsycl_here(),
        error_info{"musa_event_factory: Couldn't create event", error_code{"MUSA", err}});
    
  }
  out = evt;
  
  return make_success();
}

result musa_event_factory::destroy(musaEvent_t evt) {
  auto err = musaEventDestroy(evt);
  if (err != musaSuccess) {
    return make_error(__hipsycl_here(),
                   error_info{"musa_event_factory: Couldn't destroy event",
                              error_code{"MUSA", err}});
  }
  return make_success();
}

musa_event_pool::musa_event_pool(int device_id)
: event_pool<musa_event_factory>{musa_event_factory{device_id}} {}

}
}
