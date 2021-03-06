// Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "host.h"

#include "../common/idioms.h"
#include <cerrno>
#include <cfenv>

namespace Fortran::evaluate::host {
using namespace Fortran::parser::literals;

void HostFloatingPointEnvironment::SetUpHostFloatingPointEnvironment(
    FoldingContext &context) {
  errno = 0;
  if (feholdexcept(&originalFenv_) != 0) {
    common::die("Folding with host runtime: feholdexcept() failed: %s",
        std::strerror(errno));
    return;
  }
  if (fegetenv(&currentFenv_) != 0) {
    common::die("Folding with host runtime: fegetenv() failed: %s",
        std::strerror(errno));
    return;
  }
#if __x86_64__
  hasSubnormalFlushingHardwareControl_ = true;
  if (context.flushSubnormalsToZero()) {
    currentFenv_.__mxcsr |= 0x8000;  // result
    currentFenv_.__mxcsr |= 0x0040;  // operands
  } else {
    currentFenv_.__mxcsr &= ~0x8000;  // result
    currentFenv_.__mxcsr &= ~0x0040;  // operands
  }
#elif defined(__aarch64__)
#if defined(__GNU_LIBRARY__)
  hasSubnormalFlushingHardwareControl_ = true;
  if (context.flushSubnormalsToZero()) {
    currentFenv_.__fpcr |= (1U << 24);  // control register
  } else {
    currentFenv_.__fpcr &= ~(1U << 24);  // control register
  }
#elif defined(__BIONIC__)
  hasSubnormalFlushingHardwareControl_ = true;
  if (context.flushSubnormalsToZero()) {
    currentFenv_.__control |= (1U << 24);  // control register
  } else {
    currentFenv_.__control &= ~(1U << 24);  // control register
  }
#else
  // If F18 is built with other C libraries on AArch64, software flushing will
  // be performed around host library calls if subnormal flushing is requested
#endif
#else
  // If F18 is not built on one of the above host architecture, software
  // flushing will be performed around host library calls if needed.
#endif

#ifdef __clang__
  // clang does not ensure that floating point environment flags are meaningful.
  // It may perform optimizations that will impact the floating point
  // environment. For instance, libc++ complex float tan and tanh compilation
  // with clang -O2 introduces a division by zero on X86 in unused slots of xmm
  // registers. Therefore, fetestexcept should not be used.
  hardwareFlagsAreReliable_ = false;
#endif
  errno = 0;
  if (fesetenv(&currentFenv_) != 0) {
    common::die("Folding with host runtime: fesetenv() failed: %s",
        std::strerror(errno));
    return;
  }
  switch (context.rounding().mode) {
  case RoundingMode::TiesToEven: fesetround(FE_TONEAREST); break;
  case RoundingMode::ToZero: fesetround(FE_TOWARDZERO); break;
  case RoundingMode::Up: fesetround(FE_UPWARD); break;
  case RoundingMode::Down: fesetround(FE_DOWNWARD); break;
  case RoundingMode::TiesAwayFromZero:
    fesetround(FE_TONEAREST);
    context.messages().Say(
        "TiesAwayFromZero rounding mode is not available when folding constants"
        " with host runtime; using TiesToEven instead"_en_US);
    break;
  }
  flags_.clear();
  errno = 0;
}
void HostFloatingPointEnvironment::CheckAndRestoreFloatingPointEnvironment(
    FoldingContext &context) {
  int errnoCapture{errno};
  if (hardwareFlagsAreReliable()) {
    int exceptions{fetestexcept(FE_ALL_EXCEPT)};
    if (exceptions & FE_INVALID) {
      flags_.set(RealFlag::InvalidArgument);
    }
    if (exceptions & FE_DIVBYZERO) {
      flags_.set(RealFlag::DivideByZero);
    }
    if (exceptions & FE_OVERFLOW) {
      flags_.set(RealFlag::Overflow);
    }
    if (exceptions & FE_UNDERFLOW) {
      flags_.set(RealFlag::Underflow);
    }
    if (exceptions & FE_INEXACT) {
      flags_.set(RealFlag::Inexact);
    }
  }

  if (flags_.empty()) {
    if (errnoCapture == EDOM) {
      flags_.set(RealFlag::InvalidArgument);
    }
    if (errnoCapture == ERANGE) {
      // can't distinguish over/underflow from errno
      flags_.set(RealFlag::Overflow);
    }
  }

  if (!flags_.empty()) {
    RealFlagWarnings(context, flags_, "intrinsic function");
  }
  errno = 0;
  if (fesetenv(&originalFenv_) != 0) {
    std::fprintf(stderr, "fesetenv() failed: %s\n", std::strerror(errno));
    common::die(
        "Folding with host runtime: fesetenv() failed while restoring fenv: %s",
        std::strerror(errno));
  }
  errno = 0;
}
}
