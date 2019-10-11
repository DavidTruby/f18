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

#ifndef FORTRAN_RUNTIME_IO_H_
#define FORTRAN_RUNTIME_IO_H_

#include <cstddef>

// Prefix external names with a library version
#define F18IO(n) __F18IOa_##n

namespace Fortran::runtime {
class Descriptor;
};

namespace Fortran::runtime::IO {

class IOStatementState;
class NamelistGroup;
using Cookie = IOStatementState *;
using ExternalUnit = int;
static constexpr ExternalUnit DefaultUnit{-1};  // READ(*), WRITE(*), PRINT

extern "C" {

// These function initiate data transfer statements (READ, WRITE, PRINT).
// Internal I/O can loan the runtime library an optional block of memory
// in which to maintain state across the calls that implement the transfer;
// use of these blocks can reduce the need for dynamic memory allocation
// &/or thread-local storage.
// Example: PRINT *, 666 is implemented as
//   Cookie cookie{BeginExternalListOutput(DefaultUnit)};
//   OutputInteger64(cookie, 666);
//   EndIOStatement(cookie);

Cookie F18IO(BeginExternalListOutput)(ExternalUnit);
Cookie F18IO(BeginInternalListOutput)(char *internal, std::size_t,
                                      void *scratch, std::size_t);
Cookie F18IO(BeginExternalFormattedOutput)(ExternalUnit,
                                           const char *format, std::size_t);
Cookie F18IO(BeginInternalFormattedOutput)(char *internal, std::size_t,
    const char *format, std::size_t, void *scratch, std::size_t);
Cookie F18IO(BeginUnformattedOutput)(ExternalUnit);
Cookie F18IO(BeginNamelistOutput)(ExternalUnit, const NamelistGroup &);
// The "Input" routines have the same signatures, with const internal units.

// Control list options
void F18IO(SetADVANCE)(Cookie, const char *, std::size_t);
void F18IO(SetBLANK)(Cookie, const char *, std::size_t);
void F18IO(SetDECIMAL)(Cookie, const char *, std::size_t);
void F18IO(SetDELIM)(Cookie, const char *, std::size_t);
void F18IO(SetPAD)(Cookie, const char *, std::size_t);
void F18IO(SetPOS)(Cookie, std::int64_t);
void F18IO(SetREC)(Cookie, std::int64_t);
void F18IO(SetROUND)(Cookie, const char *, std::size_t);
void F18IO(SetSIGN)(Cookie, const char *, std::size_t);

// Data item transfer
void F18IO(OutputInteger64)(Cookie, std::int64_t);
std::int64_t F18IO(InputInteger64)(Cookie);
// &c.
void F18IO(OutputDescriptor)(Cookie, const Descriptor &);
void F18IO(InputDescriptor)(Cookie, const Descriptor &);

// Result extraction after data transfers
void F18IO(GetIOMSG)(Cookie, char *, std::size_t);
int F18IO(GetIOSTAT)(Cookie);
std::size_t F18IO(GetSIZE)(Cookie);

bool F18IO(IsEND)(Cookie);
bool F18IO(IsERR)(Cookie);
bool F18IO(IsEOR)(Cookie);

// The cookie value must not be used after calling either of these.
int F18IO(LaunchAsynchronousID)(Cookie);  // ID=
void F18IO(EndIOStatement)(Cookie);

Cookie F18IO(WaitForAsynchronous(ExternalUnit, int id);

// TODO: OPEN, INQUIRE, &c.
};
}
#endif
