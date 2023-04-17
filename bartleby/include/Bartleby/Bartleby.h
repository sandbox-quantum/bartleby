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
/// \brief Bartleby specification.
///
/// \author thb-sb

#pragma once

#include <optional>
#include <unordered_set>
#include <variant>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Object/ArchiveWriter.h"
#include "llvm/Object/Binary.h"

#include "Bartleby/Symbol.h"

namespace saq::bartleby {

/// \brief Object format.
///
/// This structure defines an object format using the minimum amount of
/// information. It uses the architecture type (`llvm::Triple::ArchType`),
/// the sub-architecture type (`llvm::Triple::SubArchType`) and the
/// object file format (`llvm::Triple::ObjectFormatType`).
struct ObjectFormat {
  /// \brief The architecture.
  llvm::Triple::ArchType arch;

  /// \brief The sub-architecture.
  llvm::Triple::SubArchType subarch;

  /// \brief The object format file.
  llvm::Triple::ObjectFormatType format_type;

  /// \brief Construct an ObjectFormat out of a `llvm::Triple`.
  ///
  /// \param triple The triple to use.
  ObjectFormat(const llvm::Triple &triple) noexcept;

  /// \brief Pack the architecture, sub-architecture and object format file
  /// enums into a 8-bytes integer.
  [[nodiscard]] uint64_t pack() const noexcept;

  /// \brief Comparison operator between two ObjectFormat.
  bool operator==(const ObjectFormat &other) const noexcept;

  /// \brief Verify that a `llvm::Triple` matches the value from an
  /// ObjectFormat.
  ///
  /// \param triple The triple to compare against the ObjectFormat.
  ///
  /// \return True if the given triple has the same architecture,
  /// sub-architecture and object format than the ones stored in ObjectFormat.
  bool matches(const llvm::Triple &triple) const noexcept;

  /// \brief Stream operator with a llvm::raw_ostream.
  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                       const ObjectFormat &format) noexcept;

  /// \brief std::hash implementation for ObjectFormat.
  struct Hash {
    inline size_t operator()(const ObjectFormat &objformat) const noexcept {
      return static_cast<size_t>(objformat.pack());
    }
  };
};

/// \brief Bartleby handle.
class Bartleby {
public:
  /// \brief Symbol map type.
  using SymbolMap = llvm::StringMap<Symbol>;

  /// \brief Constructor
  Bartleby() noexcept;

  /// \brief Copy constructor.
  Bartleby(const Bartleby &) noexcept = delete;

  /// \brief Move constructor.
  Bartleby(Bartleby &&) noexcept = default;

  /// \brief Copy assignment.
  Bartleby &operator=(const Bartleby &) noexcept = delete;

  /// \brief Move assignment.
  Bartleby &operator=(Bartleby &&) noexcept = default;

  /// \brief Destructor.
  ~Bartleby() noexcept = default;

  /// \brief Add a new binary to Bartleby.
  ///
  /// \param binary Binary.
  ///
  /// \return true if success, else false.
  [[nodiscard]] llvm::Error
  AddBinary(llvm::object::OwningBinary<llvm::object::Binary> binary) noexcept;

  /// \brief Get the map of symbols.
  ///
  /// \return The map of symbols.
  [[nodiscard]] const SymbolMap &Symbols() const noexcept { return _symbols; }

  /// \brief Prefix all global and defined symbols.
  ///
  /// \param prefix Prefix.
  ///
  /// \return The number of symbols that have been prefixed.
  size_t PrefixGlobalAndDefinedSymbols(llvm::StringRef prefix) noexcept;

  /// \brief Build final archive.
  ///
  /// \param b Bartleby handle.
  /// \param out_filepath Path to out file.
  ///
  /// \return An error, or success.
  [[nodiscard]] static llvm::Error
  BuildFinalArchive(Bartleby &&b, llvm::StringRef out_filepath) noexcept;

  /// \brief Build final archive and return a memory buffer.
  ///
  /// \param b Bartleby handle.
  ///
  /// \return Memory buffer containing the archive, or an error.
  [[nodiscard]] static llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  BuildFinalArchive(Bartleby &&b) noexcept;

private:
  /// \brief An object file.
  struct ObjectFile {
    /// \brief Handle to `llvm::object::ObjectFile`.
    ///
    llvm::object::ObjectFile *handle;

    /// \brief Owner.
    /// If the object comes from an `ObjectFile` (`.o`), then we only have
    /// the pointer to the `ObjectFile`.
    /// However, if it comes from an archive, then we get a `std::unique_ptr`
    /// out of a `llvm::object::Archive::Child` using
    /// https://llvm.org/doxygen/classllvm_1_1object_1_1Archive_1_1Child.html#a7e08f334d391b4c5c327739f3e460465.
    /// So we have to keep a track of that binary.
    std::unique_ptr<llvm::object::Binary> owner;

    /// \brief Its name.
    llvm::SmallString<32> name;
  };

  /// \brief Map of symbols.
  SymbolMap _symbols;

  /// \brief Objects.
  llvm::SmallVector<ObjectFile, 128> _objects;

  /// \brief Vector of owning binaries.
  llvm::SmallVector<llvm::object::OwningBinary<llvm::object::Binary>, 128>
      _owned_binaries;

  /// \brief The triple object format type for objects.
  ///
  /// It is not allowed to have different object format types within the same
  /// Bartleby handle.
  std::optional<ObjectFormat> _object_format;

  // Forward declaration.
  class ArchiveWriter;
};

} // end namespace saq::bartleby
