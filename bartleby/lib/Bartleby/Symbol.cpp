// Copyright 2023 SandboxAQ
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

///
/// \file
/// \brief Bartleby symbol implementation.
///
/// \author thb-sb

#include "Bartleby/Symbol.h"

#include "Bartleby/Export.h"

#include "llvm/Object/SymbolicFile.h"

#include <cassert>

#define DEBUG_TYPE "SYM"

using namespace saq::bartleby;

BARTLEBY_API Symbol::Symbol() noexcept {}

void Symbol::updateWithNewSymbolInfo(const SymbolInfo &SymInfo) noexcept {
  assert(SymInfo.Err == false);

  if ((*SymInfo.Flags & llvm::object::BasicSymbolRef::Flags::SF_Weak) == 0) {
    if ((*SymInfo.Flags & llvm::object::BasicSymbolRef::Flags::SF_Undefined) ==
        0) {
      LLVM_DEBUG(llvm::errs() << "symbol is defined. marked as defined\n");
      Defined = true;
    }
    if (*SymInfo.Flags & llvm::object::BasicSymbolRef::Flags::SF_Global) {
      LLVM_DEBUG(llvm::errs() << "symbol is global. marked as defined\n");
      Global = true;
    }
  }
  Type = SymInfo.ObjectType;
}

bool Symbol::isMachO() const noexcept {
  return Type == llvm::Triple::ObjectFormatType::MachO;
}

BARTLEBY_API void Symbol::setName(std::string Name) noexcept {
  OverwriteName = Name;
}

BARTLEBY_API bool Symbol::isGlobal() const noexcept { return Global; }

BARTLEBY_API bool Symbol::isDefined() const noexcept { return Defined; }

BARTLEBY_API std::optional<llvm::StringRef>
Symbol::getOverwriteName() const noexcept {
  return OverwriteName;
}
