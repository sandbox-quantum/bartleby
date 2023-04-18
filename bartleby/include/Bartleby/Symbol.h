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
/// \brief Bartleby library specification.
///
/// \author thb-sb

#pragma once

#include "llvm/Object/ObjectFile.h"

#include <optional>

namespace saq::bartleby {

/// Various information about a symbol.
struct SymbolInfo {
  /// \brief The symbol.
  llvm::object::SymbolRef Sym;

  /// \brief Its type.
  std::optional<llvm::object::SymbolRef::Type> Type;

  /// \brief Its flags.
  std::optional<uint32_t> Flags;

  /// \brief Its name.
  std::optional<llvm::StringRef> Name;

  /// \brief Type of the object it belongs to.
  llvm::Triple::ObjectFormatType ObjectType =
      llvm::Triple::ObjectFormatType::UnknownObjectFormat;

  /// \brief An error occurred.
  bool Err = false;
};

/// \brief An high view of a symbol.
class Symbol {
public:
  /// \brief Returns the visibility of the symbol.
  ///
  /// \returns True if symbol is global, else false.
  [[nodiscard]] bool isGlobal() const noexcept;

  /// \brief Returns the definedness of the symbol.
  ///
  /// \returns True if symbol is defined, else false.
  [[nodiscard]] bool isDefined() const noexcept;

  /// \brief Counts how many times this symbol is referenced.
  ///
  /// \returns Number of references.
  [[nodiscard]] size_t getReferences() const noexcept;

  /// \brief Returns the overwrite name.
  ///
  /// \returns The overwrite name.
  [[nodiscard]] std::optional<llvm::StringRef>
  getOverwriteName() const noexcept;

  /// \brief Returns true if the symbol contains references to some mach-o
  /// symbols.
  ///
  /// \returns True if the symbol contains references to some mach-o, else
  /// false.
  [[nodiscard]] bool isMachO() const noexcept;

  /// \brief Sets the name of the symbol.
  ///
  /// \param name Name to set.
  void setName(std::string Name) noexcept;

  /// \brief Updates the symbol with new symbol information.
  ///
  /// \param syminfo Symbol information.
  void updateWithNewSymbolInfo(const SymbolInfo &Syminfo) noexcept;

  /// \brief Constructs a new symbol.
  Symbol() noexcept;

  Symbol(const Symbol &) noexcept = delete;
  Symbol(Symbol &&) noexcept = default;
  Symbol &operator=(const Symbol &) noexcept = delete;
  Symbol &operator=(Symbol &&) noexcept = default;
  ~Symbol() noexcept = default;

private:
  /// \brief New name to set, if any.
  std::optional<std::string> OverwriteName;

  /// \brief Type of the object it belongs to.
  llvm::Triple::ObjectFormatType Type =
      llvm::Triple::ObjectFormatType::UnknownObjectFormat;

  /// \brief Is global.
  bool Global = false;

  /// \brief Is defined.
  bool Defined = false;
};

} // end namespace saq::bartleby
