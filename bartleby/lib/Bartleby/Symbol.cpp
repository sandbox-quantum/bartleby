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

namespace saq::bartleby {

BARTLEBY_API Symbol::Symbol() noexcept {}

void Symbol::UpdateWithNewSymbolInfo(const SymbolInfo &syminfo) noexcept {
  assert(syminfo.err == false);

  if ((*syminfo.flags & llvm::object::BasicSymbolRef::Flags::SF_Weak) == 0) {
    if ((*syminfo.flags & llvm::object::BasicSymbolRef::Flags::SF_Undefined) ==
        0) {
      LLVM_DEBUG(llvm::errs() << "symbol is defined. marked as defined\n");
      _defined = true;
    }
    if (*syminfo.flags & llvm::object::BasicSymbolRef::Flags::SF_Global) {
      LLVM_DEBUG(llvm::errs() << "symbol is global. marked as defined\n");
      _global = true;
    }
  }
  _type = syminfo.object_type;
}

bool Symbol::IsMachO() const noexcept {
  return _type == llvm::Triple::ObjectFormatType::MachO;
}

BARTLEBY_API void Symbol::SetName(std::string name) noexcept {
  _overwrite_name = name;
}

BARTLEBY_API bool Symbol::Global() const noexcept { return _global; }

BARTLEBY_API bool Symbol::Defined() const noexcept { return _defined; }

BARTLEBY_API std::optional<llvm::StringRef>
Symbol::OverwriteName() const noexcept {
  return _overwrite_name;
}

} // end namespace saq::bartleby
