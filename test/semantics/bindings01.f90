! Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
!
! Licensed under the Apache License, Version 2.0 (the "License");
! you may not use this file except in compliance with the License.
! You may obtain a copy of the License at
!
!     http://www.apache.org/licenses/LICENSE-2.0
!
! Unless required by applicable law or agreed to in writing, software
! distributed under the License is distributed on an "AS IS" BASIS,
! WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
! See the License for the specific language governing permissions and
! limitations under the License.

! Confirm enforcement of constraints and restrictions in 7.5.7.3
! and C779-C785.

module m
  !ERROR: An ABSTRACT derived type must be extensible
  type, abstract, bind(c) :: badAbstract1
  end type
  !ERROR: An ABSTRACT derived type must be extensible
  type, abstract :: badAbstract2
    sequence
  end type
  type, abstract :: abstract
   contains
    !ERROR: DEFERRED is required when an interface-name is provided
    procedure(s1), pass :: ab1
    !ERROR: Type-bound procedure 'ab3' may not be both DEFERRED and NON_OVERRIDABLE
    procedure(s1), deferred, non_overridable :: ab3
    !ERROR: DEFERRED is only allowed when an interface-name is provided
    procedure, deferred, non_overridable :: ab4 => s1
  end type
  type :: nonoverride
   contains
    procedure, non_overridable, nopass :: no1 => s1
  end type
  type, extends(nonoverride) :: nonoverride2
  end type
  type, extends(nonoverride2) :: nonoverride3
   contains
    !ERROR: Override of NON_OVERRIDABLE 'no1' is not permitted
    procedure, nopass :: no1 => s1
  end type
  type, abstract :: missing
   contains
    procedure(s4), deferred :: am1
  end type
  !ERROR: Non-ABSTRACT extension of ABSTRACT derived type 'missing' lacks a binding for DEFERRED procedure 'am1'
  type, extends(missing) :: concrete
  end type
  type, extends(missing) :: intermediate
   contains
    procedure :: am1 => s7
  end type
  type, extends(intermediate) :: concrete2  ! ensure no false missing binding error
  end type
  type, bind(c) :: inextensible1
  end type
  !ERROR: The parent type is not extensible
  type, extends(inextensible1) :: badExtends1
  end type
  type :: inextensible2
    sequence
  end type
  !ERROR: The parent type is not extensible
  type, extends(inextensible2) :: badExtends2
  end type
  !ERROR: Derived type 'real' not found
  type, extends(real) :: badExtends3
  end type
  type :: base
    real :: component
   contains
    !ERROR: Procedure bound to non-ABSTRACT derived type 'base' may not be DEFERRED
    procedure(s2), deferred :: bb1
    !ERROR: DEFERRED is only allowed when an interface-name is provided
    procedure, deferred :: bb2 => s2
  end type
  type, extends(base) :: extension
   contains
     !ERROR: A type-bound procedure binding may not have the same name as a parent component
     procedure :: component => s3
  end type
  type :: nopassBase
   contains
    procedure, nopass :: tbp => s1
  end type
  type, extends(nopassBase) :: passExtends
   contains
    !ERROR: A passed-argument type-bound procedure may not override a NOPASS procedure
    procedure :: tbp => s5
  end type
  type :: passBase
   contains
    procedure :: tbp => s6
  end type
  type, extends(passBase) :: nopassExtends
   contains
    !ERROR: A NOPASS type-bound procedure may not override a passed-argument procedure
    procedure, nopass :: tbp => s1
  end type
 contains
  subroutine s1(x)
    class(abstract), intent(in) :: x
  end subroutine s1
  subroutine s2(x)
    class(base), intent(in) :: x
  end subroutine s2
  subroutine s3(x)
    class(extension), intent(in) :: x
  end subroutine s3
  subroutine s4(x)
    class(missing), intent(in) :: x
  end subroutine s4
  subroutine s5(x)
    class(passExtends), intent(in) :: x
  end subroutine s5
  subroutine s6(x)
    class(passBase), intent(in) :: x
  end subroutine s6
  subroutine s7(x)
    class(intermediate), intent(in) :: x
  end subroutine s7
end module

