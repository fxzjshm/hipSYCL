/*
 * This file is part of hipSYCL, a SYCL implementation based on CUDA/HIP
 *
 * Copyright (c) 2019-2022 Aksel Alpay
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

#include "hipSYCL/sycl/libkernel/sscp/builtins/math.hpp"
#include "hipSYCL/sycl/libkernel/sscp/builtins/builtin_config.hpp"
#include "hipSYCL/sycl/libkernel/sscp/builtins/musa/builtin.hpp"

#define PI 3.14159265358979323846

#define HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(name, mtml_name)                   \
                                                                               \
  HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_##name##_f32(float x) {            \
    return mtml_name##_f32(x);                                                 \
  }                                                                            \
  HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_##name##_f64(double x) {          \
    return mtml_name##_f64(x);                                                 \
  }

#define HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(name, mtml_name)                  \
                                                                               \
  HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_##name##_f32(float x, float y) {   \
    return mtml_name##_f32(x, y);                                              \
  }                                                                            \
  HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_##name##_f64(double x,            \
                                                          double y) {          \
    return mtml_name##_f64(x, y);                                              \
  }

#define HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN3(name, mtml_name)                  \
                                                                               \
  HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_##name##_f32(float x, float y,     \
                                                         float z) {            \
    return mtml_name##_f32(x, y, z);                                           \
  }                                                                            \
  HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_##name##_f64(double x, double y,  \
                                                          double z) {          \
    return mtml_name##_f64(x, y, z);                                           \
  }

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(acos, __mt_acos)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(acosh, __mt_acosh)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_acospi_f32(float x) { return __hipsycl_sscp_acos_f32(x) / PI; }
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_acospi_f64(double x) { return __hipsycl_sscp_acos_f64(x) / PI; }

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(asin, __mt_asin)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(asinh, __mt_asinh)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_asinpi_f32(float x) { return __hipsycl_sscp_asin_f32(x) / PI; }
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_asinpi_f64(double x) { return __hipsycl_sscp_asin_f64(x) / PI; }

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(atan, __mt_atan)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(atan2, __mt_atan2)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(atanh, __mt_atan)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_atanpi_f32(float x) { return __hipsycl_sscp_atan_f32(x) / PI; }
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_atanpi_f64(double x) { return __hipsycl_sscp_atan_f64(x) / PI; }

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_atan2pi_f32(float x, float y) { return __hipsycl_sscp_atan2_f32(x, y) / PI; }
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_atan2pi_f64(double x, double y) { return __hipsycl_sscp_atan2_f64(x, y) / PI; }

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(cbrt, __mt_cbrt)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(ceil, __mt_ceil)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(copysign, __mt_copysign)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(cos, __mt_cos)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(cosh, __mt_cosh)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(cospi, __mt_cospi)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(erf, __mt_erf)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(erfc, __mt_erfc)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(exp, __mt_exp)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(exp2, __mt_exp2)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_exp10_f32(float x) { return __hipsycl_sscp_powr_f32(10.0f, x); }
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_exp10_f64(double x) { return __mt_exp10_f64(x); }

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(pow, __mt_pow)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(expm1, __mt_expm1)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(fabs, __mt_fabs)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(fdim, __mt_fdim)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(floor, __mt_floor)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN3(fma, __mt_fma)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(fmax, __mt_fmax)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(fmin, __mt_fmin)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(fmod, __mt_fmod)

// fmin(x - floor(x), nextafter(genfloat(1.0), genfloat(0.0)) ). floor(x) is returned in iptr.
HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_fract_f32(float x, float* y ) {
  *y = __hipsycl_sscp_floor_f32(x);
  return __hipsycl_sscp_fmin_f32(x - *y, __hipsycl_sscp_nextafter_f32(1.f, 0.f));
}
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_fract_f64(double x, double* y) {
  *y = __hipsycl_sscp_floor_f64(x);
  return __hipsycl_sscp_fmin_f64(x - *y, __hipsycl_sscp_nextafter_f64(1.f, 0.f));
}

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_frexp_f32(float x,
                                                    __hipsycl_int32 *y) {
  return __mt_frexp_f32(x, y);
}
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_frexp_f64(double x,
                                                     __hipsycl_int64 *y) {
  __hipsycl_int32 w;
  double res = __mt_frexp_f64(x, &w);
  *y = static_cast<__hipsycl_int64>(w);
  return res;
}

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(hypot, __mt_hypot)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(ilogb, __mt_ilogb)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_ldexp_f32(float x,
                                                    __hipsycl_int32 k) {
  return __mt_ldexp_f32(x, k);
}

HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_ldexp_f64(double x,
                                                     __hipsycl_int64 k) {
  return __mt_ldexp_f64(x, k);
}

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(tgamma, __mt_tgamma)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(lgamma, __mt_lgamma)


HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_lgamma_r_f32(float x, __hipsycl_int32* y ) {
  auto r = __hipsycl_sscp_lgamma_f32(x);
  auto g = __hipsycl_sscp_tgamma_f32(x);
  *y = (g >= 0) ? 1 : -1;
  return r;
}

HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_lgamma_r_f64(double x, __hipsycl_int64* y) {
  auto r = __hipsycl_sscp_lgamma_f64(x);
  auto g = __hipsycl_sscp_tgamma_f64(x);
  *y = (g >= 0) ? 1 : -1;
  return r;
}

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(log, __mt_log)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(log2, __mt_log2)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(log10, __mt_log10)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(log1p, __mt_log1p)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(logb, __mt_logb)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN3(mad, __mt_fma)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_maxmag_f32(float x, float y) {
  auto abs_x = __hipsycl_sscp_fabs_f32(x);
  auto abs_y = __hipsycl_sscp_fabs_f32(y);
  if(abs_x == abs_y) return __hipsycl_sscp_fmax_f32(x,y);
  return (abs_x > abs_y) ? x : y;
}

HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_maxmag_f64(double x, double y) {
  auto abs_x = __hipsycl_sscp_fabs_f64(x);
  auto abs_y = __hipsycl_sscp_fabs_f64(y);
  if(abs_x == abs_y) return __hipsycl_sscp_fmax_f64(x,y);
  return (abs_x > abs_y) ? x : y;
}

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_minmag_f32(float x, float y) {
  auto abs_x = __hipsycl_sscp_fabs_f32(x);
  auto abs_y = __hipsycl_sscp_fabs_f32(y);
  if(abs_x == abs_y) return __hipsycl_sscp_fmin_f32(x,y);
  return (abs_x < abs_y) ? x : y;
}

HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_minmag_f64(double x, double y) {
  auto abs_x = __hipsycl_sscp_fabs_f64(x);
  auto abs_y = __hipsycl_sscp_fabs_f64(y);
  if(abs_x == abs_y) return __hipsycl_sscp_fmin_f64(x,y);
  return (abs_x < abs_y) ? x : y;
}

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_modf_f32(float x, float* y ) {
  return __mt_modf_f32(x, y);
}
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_modf_f64(double x, double* y) {
  return __mt_modf_f64(x, y);
}

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(nextafter, __mt_nextafter)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(powr, __mt_powr)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_pown_f32(float x, __hipsycl_int32 y) {
  return __mt_pown_f32(x, y);
}
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_pown_f64(double x,
                                                    __hipsycl_int64 y) {
  return __mt_pown_f64(x, static_cast<__hipsycl_int32>(y));
}

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN2(remainder, __mt_remainder)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(rint, __mt_rint)

HIPSYCL_SSCP_BUILTIN float __hipsycl_sscp_rootn_f32(float x, __hipsycl_int32 y) { return __hipsycl_sscp_pow_f32(x, 1.f/y); }
HIPSYCL_SSCP_BUILTIN double __hipsycl_sscp_rootn_f64(double x, __hipsycl_int64 y) {return __hipsycl_sscp_pow_f64(x, 1./y); }

HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(round, __mt_round)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(rsqrt, __mt_rsqrt)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(sqrt, __mt_sqrt)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(sin, __mt_sin)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(sinh, __mt_sinh)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(sinpi, __mt_sinpi)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(tan, __mt_tan)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(tanh, __mt_tanh)
HIPSYCL_SSCP_MAP_MTML_FLOAT_BUILTIN(trunc, __mt_trunc)
