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

#include <optional>

#include "llvm/Object/ObjectFile.h"

namespace saq::bartleby {

/// Various information about a symbol.
struct SymbolInfo {
  /// \brief The symbol.
  llvm::object::SymbolRef sym;

  /// \brief Its type.
  std::optional<llvm::object::SymbolRef::Type> type;

  /// \brief Its flags.
  std::optional<uint32_t> flags;

  /// \brief Its name.
  std::optional<llvm::StringRef> name;

  /// \brief Type of the object it belongs to.
  llvm::Triple::ObjectFormatType object_type =
      llvm::Triple::ObjectFormatType::UnknownObjectFormat;

  /// \brief An error occurred.
  bool err = false;
};

/// \brief An high view of a symbol.
class Symbol {
public:
  /// \brief Returns the visibility of the symbol.
  ///
  /// \returns True if symbol is global, else false.
  [[nodiscard]] bool Global() const noexcept;

  /// \brief Returns the definedness of the symbol.
  ///
  /// \returns True if symbol is defined, else false.
  [[nodiscard]] bool Defined() const noexcept;

  /// \brief Counts how many times this symbol is referenced.
  ///
  /// \returns Number of references.
  [[nodiscard]] size_t References() const noexcept;

  /// \brief Returns the overwrite name.
  ///
  /// \returns The overwrite name.
  [[nodiscard]] std::optional<llvm::StringRef> OverwriteName() const noexcept;

  /// \brief Returns true if the symbol contains references to some mach-o
  /// symbols.
  ///
  /// \returns True if the symbol contains references to some mach-o, else
  /// false.
  [[nodiscard]] bool IsMachO() const noexcept;

  /// \brief Sets the name of the symbol.
  ///
  /// \param name Name to set.
  void SetName(std::string name) noexcept;

  /// \brief Updates the symbol with new symbol information.
  ///
  /// \param syminfo Symbol information.
  void UpdateWithNewSymbolInfo(const SymbolInfo &syminfo) noexcept;

  /// \brief Constructs a new symbol.
  Symbol() noexcept;

  Symbol(const Symbol &) noexcept = delete;
  Symbol(Symbol &&) noexcept = default;
  Symbol &operator=(const Symbol &) noexcept = delete;
  Symbol &operator=(Symbol &&) noexcept = default;
  ~Symbol() noexcept = default;

private:
  /// \brief New name to set, if any.
  std::optional<std::string> _overwrite_name;

  /// \brief Type of the object it belongs to.
  llvm::Triple::ObjectFormatType _type =
      llvm::Triple::ObjectFormatType::UnknownObjectFormat;

  /// \brief Is global.
  bool _global = false;

  /// \brief Is defined.
  bool _defined = false;
};

} // end namespace saq::bartleby
