//===-- lib/semantics/check-return.cc -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "check-return.h"
#include "flang/common/Fortran-features.h"
#include "flang/parser/message.h"
#include "flang/parser/parse-tree.h"
#include "flang/semantics/semantics.h"
#include "flang/semantics/tools.h"

namespace Fortran::semantics {

static const Scope *FindContainingSubprogram(const Scope &start) {
  const Scope *scope{FindProgramUnitContaining(start)};
  return scope &&
          (scope->kind() == Scope::Kind::MainProgram ||
              scope->kind() == Scope::Kind::Subprogram)
      ? scope
      : nullptr;
}

void ReturnStmtChecker::Leave(const parser::ReturnStmt &returnStmt) {
  // R1542 Expression analysis validates the scalar-int-expr
  // C1574 The return-stmt shall be in the inclusive scope of a function or
  // subroutine subprogram.
  // C1575 The scalar-int-expr is allowed only in the inclusive scope of a
  // subroutine subprogram.
  const auto &scope{context_.FindScope(context_.location().value())};
  if (const auto *subprogramScope{FindContainingSubprogram(scope)}) {
    if (returnStmt.v &&
        (subprogramScope->kind() == Scope::Kind::MainProgram ||
            IsFunction(*subprogramScope->GetSymbol()))) {
      context_.Say(
          "RETURN with expression is only allowed in SUBROUTINE subprogram"_err_en_US);
    } else if (context_.ShouldWarn(common::LanguageFeature::ProgramReturn)) {
      context_.Say("RETURN should not appear in a main program"_en_US);
    }
  }
}

}
