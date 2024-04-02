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

#include "Bartleby/Symbol.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Object/Binary.h"

#include <unordered_set>
#include <variant>

namespace saq::bartleby {

/// \brief Object format.
///
/// This structure defines an object format using the minimum amount of
/// information. It uses the architecture type (\p llvm::Triple::ArchType),
/// the sub-architecture type (\p llvm::Triple::SubArchType) and the
/// object file format (\p llvm::Triple::ObjectFormatType).
struct ObjectFormat {
  /// \brief The architecture.
  llvm::Triple::ArchType Arch;

  /// \brief The sub-architecture.
  llvm::Triple::SubArchType SubArch;

  /// \brief The object format file.
  llvm::Triple::ObjectFormatType FormatType;

  /// \brief Constructs an \p ObjectFormat out of a \p llvm::Triple.
  ///
  /// \param Triple The triple to use.
  ObjectFormat(const llvm::Triple &Triple) noexcept;

  /// \brief Packs the architecture, sub-architecture and object format file
  /// enums into a 8-bytes unsigned integer.
  ///
  /// \returns The packed values as a 8-bytes unsigned integer.
  [[nodiscard]] uint64_t pack() const noexcept;

  bool operator==(const ObjectFormat &Other) const noexcept;

  /// \brief Verifies that a \p llvm::Triple matches the value from an
  /// \p ObjectFormat.
  ///
  /// \param Triple The triple to compare against the ObjectFormat.
  ///
  /// \returns True if the given triple has the same architecture,
  /// sub-architecture and object format than the ones stored in ObjectFormat.
  bool matches(const llvm::Triple &Triple) const noexcept;

  /// \brief Dumps the \p ObjectFormat to a \p llvm::raw_ostream.
  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                       const ObjectFormat &Format) noexcept;

  /// \brief \p std::hash implementation for \p ObjectFormat.
  struct Hash {
    inline size_t operator()(const ObjectFormat &ObjFormat) const noexcept {
      return static_cast<size_t>(ObjFormat.pack());
    }
  };
};

/// \brief Bartleby handle.
class Bartleby {
public:
  /// \brief Symbol map type.
  using SymbolMap = llvm::StringMap<Symbol>;

  /// \brief Constructs an empty Bartleby handle.
  Bartleby() noexcept;

  Bartleby(const Bartleby &) noexcept = delete;
  Bartleby(Bartleby &&) noexcept = default;
  Bartleby &operator=(const Bartleby &) noexcept = delete;
  Bartleby &operator=(Bartleby &&) noexcept = default;
  ~Bartleby() noexcept = default;

  /// \brief Adds a new binary to Bartleby.
  ///
  /// \param[in] Binary Binary.
  ///
  /// \returns An error.
  [[nodiscard]] llvm::Error
  addBinary(llvm::object::OwningBinary<llvm::object::Binary> Binary) noexcept;

  /// \brief Gets a const reference to the map of symbols.
  ///
  /// \returns The map of symbols.
  [[nodiscard]] const SymbolMap &getSymbols() const noexcept { return Symbols; }

  /// \brief Applies a prefix to all global and defined symbols.
  ///
  /// \param Prefix Prefix.
  ///
  /// \returns The number of symbols that have been prefixed.
  size_t prefixGlobalAndDefinedSymbols(llvm::StringRef Prefix) noexcept;

  /// \brief Builds the final archive and writes its content to a file.
  ///
  /// \param[in] B Bartleby handle.
  /// \param OutFilepath Path to out file.
  ///
  /// \returns An error.
  [[nodiscard]] static llvm::Error
  buildFinalArchive(Bartleby &&B, llvm::StringRef OutFilepath) noexcept;

  /// \brief Builds the final archive and returns its content.
  ///
  /// \param[in] B Bartleby handle.
  ///
  /// \returns The memory buffer containing the archive, or an error.
  [[nodiscard]] static llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  buildFinalArchive(Bartleby &&B) noexcept;

private:
  /// \brief An object file.
  struct ObjectFile {
    /// \brief Handle to \p llvm::object::ObjectFile.
    llvm::object::ObjectFile *Handle;

    /// \brief Owner.
    /// If the object comes from an \p ObjectFile (\p .o), then we only have
    /// the pointer to the \p ObjectFile.
    /// However, if it comes from an archive, then we get a \p std::unique_ptr
    /// out of a \p llvm::object::Archive::Child using
    /// https://llvm.org/doxygen/classllvm_1_1object_1_1Archive_1_1Child.html#a7e08f334d391b4c5c327739f3e460465.
    /// We therefore have to keep a track of that binary.
    std::unique_ptr<llvm::object::Binary> Owner;

    /// \brief Its name.
    llvm::SmallString<32> Name;

    /// \brief The object native alignment.
    ///
    /// This is used for writing fat Mach-O archives.
    uint32_t Alignment;
  };

  /// \brief A set of object formats.
  using ObjectFormatSet = std::unordered_set<ObjectFormat, ObjectFormat::Hash>;

  /// \brief Type for the current object format.
  ///
  /// This is either an \p ObjectFormat, or a set of \p ObjectFormat.
  ///
  /// The set variant is used when the handle is dealing with fat Mach-O.
  using ObjectFormatVariant =
      std::variant<std::monostate, ObjectFormat, ObjectFormatSet>;

  /// \brief Returns true if the input objects have to be fat Mach-O.
  ///
  /// \returns True if at least one input object is a fat Mach-O.
  [[nodiscard]] bool isMachOUniversalBinary() const noexcept;

  /// \brief Verifies that an given object format matches the one the handle
  /// actually handles.
  ///
  /// \param ObjFmt Object format to compare against.
  ///
  /// If no object format is specified (i.e. \p std::monostate), true is
  /// returned.
  ///
  /// \returns true if this is a match, or if no object format was saved before.
  [[nodiscard]] bool
  objectFormatMatches(const ObjectFormat &ObjFmt) const noexcept;

  /// \brief Adds a fat Mach-O, aka a Universal Mach-O Binary.
  ///
  /// \param[in] OwningBinary The owning binary containing the fat Mach-O.
  ///
  /// \returns An error.
  [[nodiscard]] llvm::Error addMachOUniversalBinary(
      llvm::object::OwningBinary<llvm::object::Binary> OwningBinary) noexcept;

  /// \brief Map of symbols.
  SymbolMap Symbols;

  /// \brief Objects.
  llvm::SmallVector<ObjectFile, 128> Objects;

  /// \brief Vector of owning binaries.
  llvm::SmallVector<llvm::object::OwningBinary<llvm::object::Binary>, 128>
      OwnedBinaries;

  /// \brief The triple object format type for objects.
  ///
  /// It is not allowed to have different object format types within the same
  /// Bartleby handle.
  ObjectFormatVariant ObjFormat;

  // Forward declaration.
  class ArchiveWriter;
};

} // end namespace saq::bartleby
