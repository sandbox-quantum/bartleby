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
/// \brief Bartleby implementation.
///
/// \author thb-sb

#include "Bartleby/Bartleby.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ObjCopy/COFF/COFFConfig.h"
#include "llvm/ObjCopy/CommonConfig.h"
#include "llvm/ObjCopy/ELF/ELFConfig.h"
#include "llvm/ObjCopy/MachO/MachOConfig.h"
#include "llvm/ObjCopy/MultiFormatConfig.h"
#include "llvm/ObjCopy/ObjCopy.h"
#include "llvm/ObjCopy/XCOFF/XCOFFConfig.h"
#include "llvm/ObjCopy/wasm/WasmConfig.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/ArchiveWriter.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"

#include "Bartleby/Error.h"
#include "Bartleby/Export.h"
#include "Bartleby/Symbol.h"

#define DEBUG_TYPE "bartleby"

namespace saq::bartleby {

BARTLEBY_API Bartleby::Bartleby() noexcept = default;

namespace {

/// \brief Fetch various information from a symbol.
///
/// \param sym Symbol.
///
/// \return Informations.
[[nodiscard]] SymbolInfo
getSymbolInfo(const llvm::object::SymbolRef &sym) noexcept {
  SymbolInfo info{.sym = sym};

  if (auto err = sym.getName().moveInto(info.name)) {
    LLVM_DEBUG(llvm::dbgs() << "failed to get name: " << err << '\n');
    info.err = true;
  }
  if (auto err = sym.getFlags().moveInto(info.flags)) {
    LLVM_DEBUG(llvm::dbgs() << "failed to get flags: " << err << '\n');
    info.err = true;
  }
  if (auto err = sym.getType().moveInto(info.type)) {
    LLVM_DEBUG(llvm::dbgs() << "failed to get type: " << err << '\n');
    info.err = true;
  }
  return info;
}

/// \brief Collect all symbol infos from an object.
///
/// \param obj Object.
/// \param syminfos Container where to store the symbol infos.
void collectSymbolInfos(const llvm::object::ObjectFile *obj,
                        llvm::SmallVectorImpl<SymbolInfo> &syminfos) noexcept {
  for (const auto &sym : obj->symbols()) {
    auto &info = syminfos.emplace_back(getSymbolInfo(sym));
    info.object_type = obj->getTripleObjectFormat();
  }
}

/// \brief Determine if we should skip a symbol based on its info.
///
/// \param syminfo Symbol information.
///
/// \return true if we should skip it, else false.
[[nodiscard]] bool shouldSkipSymbol(const SymbolInfo &syminfo) noexcept {
  if (syminfo.err) {
    LLVM_DEBUG(llvm::dbgs()
               << "failed to get all info for symbol. Skipping it\n");
    return true;
  }

  switch (*syminfo.type) {
  case llvm::object::SymbolRef::Type::ST_Other:
  case llvm::object::SymbolRef::Type::ST_Debug:
  case llvm::object::SymbolRef::Type::ST_File: {
    LLVM_DEBUG(llvm::dbgs()
               << "skipping " << syminfo.name.value().data()
               << " because it isn't a function or an unknown symbol\n");
    return true;
  }
  default: {
  }
  }
  return false;
}

/// \brief Process an object file.
///
/// \param object The object file.
/// \param symbols Symbol map to update.
void ProcessObjectFile(const llvm::object::ObjectFile *object,
                       Bartleby::SymbolMap &symbols) {
  llvm::SmallVector<SymbolInfo, 128> syminfos;
  collectSymbolInfos(object, syminfos);
  for (const auto &syminfo : syminfos) {
    if (shouldSkipSymbol(syminfo)) {
      continue;
    }

    std::string name(*syminfo.name);

    LLVM_DEBUG(llvm::dbgs()
               << "Found symbol '" << name << "', type: " << *syminfo.type
               << ", flags: " << *syminfo.flags << '\n');
    auto &sym = symbols[name];
    sym.UpdateWithNewSymbolInfo(syminfo);
  }
}

} // end anonymous namespace

ObjectFormat::ObjectFormat(const llvm::Triple &triple) noexcept
    : arch(triple.getArch()), subarch(triple.getSubArch()),
      format_type(triple.getObjectFormat()) {}

uint64_t ObjectFormat::pack() const noexcept {
  return (static_cast<uint64_t>(arch)) |
         (static_cast<uint64_t>(subarch) << 16) |
         (static_cast<uint64_t>(format_type) << 32);
}

bool ObjectFormat::operator==(const ObjectFormat &other) const noexcept {
  return pack() == other.pack();
}

bool ObjectFormat::matches(const llvm::Triple &triple) const noexcept {
  return ObjectFormat{triple}.pack() == pack();
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              const ObjectFormat &object_format) noexcept {
  return os << "ObjectFormat(arch=" << object_format.arch
            << ", subarch=" << object_format.subarch
            << ", file format=" << object_format.format_type << ')';
}

BARTLEBY_API llvm::Error Bartleby::AddBinary(
    llvm::object::OwningBinary<llvm::object::Binary> owning_binary) noexcept {
  auto *binary = owning_binary.getBinary();
  llvm::Error e = llvm::Error::success();

  const auto type = binary->getTripleObjectFormat();
  if (auto *obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary)) {
    const auto triple = obj->makeTriple();
    if (!_object_format) {
      _object_format = triple;
    }
    if (!_object_format->matches(triple)) {
      return llvm::make_error<Error>(Error::ObjectFormatTypeMismatchReason{
          .constraint = *_object_format, .found = ObjectFormat{triple}});
    }
    ProcessObjectFile(obj, _symbols);
    auto &entry = _objects.emplace_back(ObjectFile{.handle = obj});
    (llvm::Twine(llvm::utostr(_objects.size())) + ".o")
        .toNullTerminatedStringRef(entry.name);
  } else if (auto *archive = llvm::dyn_cast<llvm::object::Archive>(binary)) {
    llvm::Error e = llvm::Error::success();
    for (const auto &ch : archive->children(e)) {
      auto bin_or_err = ch.getAsBinary();
      if (!bin_or_err) {
        return bin_or_err.takeError();
      }
      if (auto *obj = llvm::dyn_cast<llvm::object::ObjectFile>(
              bin_or_err.get().get())) {
        ProcessObjectFile(obj, _symbols);
        auto &entry = _objects.emplace_back(ObjectFile{
            .owner = std::move(bin_or_err.get()),
        });
        entry.handle = obj;
        if (auto name_or_err = ch.getName()) {
          entry.name = name_or_err.get();
        } else {
          (llvm::Twine(llvm::utostr(_objects.size())) + ".o")
              .toNullTerminatedStringRef(entry.name);
        }
      } else {
        Error::UnsupportedBinaryReason reason;
        llvm::raw_svector_ostream os(reason.msg);
        os << "unsupported binary '" << binary->getType()
           << "' (triple: " << binary->getTripleObjectFormat() << ')';
        return llvm::make_error<Error>(std::move(reason));
      }
    }
  } else {
    Error::UnsupportedBinaryReason reason;
    llvm::raw_svector_ostream os(reason.msg);
    os << "unsupported binary '" << binary->getType()
       << "' (triple: " << binary->getTripleObjectFormat() << ')';
    return llvm::make_error<Error>(std::move(reason));
  }
  _owned_binaries.push_back(std::move(owning_binary));
  return llvm::Error::success();
}

BARTLEBY_API size_t
Bartleby::PrefixGlobalAndDefinedSymbols(llvm::StringRef prefix) noexcept {
  size_t n = 0;
  const auto end = _symbols.end();
  for (auto entry = _symbols.begin(); entry != end; ++entry) {
    const auto &name = entry->first();
    auto &sym = entry->getValue();
    if (sym.Global() && sym.Defined()) {
      std::string new_name;
      if (sym.IsMachO()) {
        new_name += '_';
      }
      new_name += prefix;

      if (sym.IsMachO()) {
        new_name += name.substr(1);
      } else {
        new_name += name;
      }
      sym.SetName(std::move(new_name));
      ++n;
    }
  }

  return n;
}

/// \brief Archive builder that implements out multi format config.
///
/// See https://llvm.org/doxygen/classllvm_1_1objcopy_1_1MultiFormatConfig.html.
class Bartleby::ArchiveBuilder : public llvm::objcopy::MultiFormatConfig {
public:
  /// \brief Constructor using a Bartleby handle.
  ///
  /// \param b Bartleby handle.
  ArchiveBuilder(Bartleby &&b) noexcept : _b{std::move(b)} {
    const auto end = _b._symbols.end();
    for (auto entry = _b._symbols.begin(); entry != end; ++entry) {
      const auto &name = entry->first();
      const auto &sym = entry->getValue();
      if (const auto oname = sym.OverwriteName(); oname) {
        LLVM_DEBUG(llvm::dbgs() << "bartleby is going to rename '" << name
                                << "' into '" << *oname << "'\n");
        _common_config.SymbolsToRename[name] = *oname;
      }
    }
  }

  /// \brief Build the final archive.
  ///
  /// \param out_filepath Path to out file.
  ///
  /// \return An error.
  [[nodiscard]] llvm::Error Build(llvm::StringRef out_filepath) noexcept {
    if (auto err = ExecuteObjCopyOnObjects(); err) {
      return err;
    }

    return llvm::writeArchive(out_filepath, _ar_members,
                              /* WriteSymtab= */ true,
                              _ar_members[0].detectKindFromObject(),
                              /* Deterministic= */ true,
                              /* Thin= */ false);
  }

  /// \brief Build the final archive.
  ///
  /// \return A memory buffer or an error.
  [[nodiscard]] llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  Build() noexcept {
    if (auto err = ExecuteObjCopyOnObjects(); err) {
      return err;
    }

    return llvm::writeArchiveToBuffer(_ar_members,
                                      /* WriteSymtab= */ true,
                                      _ar_members[0].detectKindFromObject(),
                                      /* Deterministic= */ true,
                                      /* Thin= */ false);
  }

  /// \brief Destructor.
  ~ArchiveBuilder() noexcept override = default;

  const llvm::objcopy::CommonConfig &getCommonConfig() const noexcept override {
    return _common_config;
  }

  llvm::Expected<const llvm::objcopy::ELFConfig &>
  getELFConfig() const noexcept override {
    return _elf_config;
  }

  llvm::Expected<const llvm::objcopy::COFFConfig &>
  getCOFFConfig() const noexcept override {
    return _coff_config;
  }

  llvm::Expected<const llvm::objcopy::MachOConfig &>
  getMachOConfig() const noexcept override {
    return _macho_config;
  }

  llvm::Expected<const llvm::objcopy::WasmConfig &>
  getWasmConfig() const noexcept override {
    return _wasm_config;
  }

  llvm::Expected<const llvm::objcopy::XCOFFConfig &>
  getXCOFFConfig() const noexcept override {
    return _xcoff_config;
  }

private:
  /// \brief Execute objcopy on objects that belongs to the Bartleby handle.
  ///
  /// \return An error.
  [[nodiscard]] llvm::Error ExecuteObjCopyOnObjects() noexcept {
    LLVM_DEBUG(llvm::dbgs()
               << "processing " << _b._objects.size() << " object(s)\n");
    for (auto &obj : _b._objects) {
      llvm::SmallVector<char, 8192> content;
      llvm::raw_svector_ostream stream(content);
      if (auto err = llvm::objcopy::executeObjcopyOnBinary(*this, *obj.handle,
                                                           stream)) {
        return err;
      }
      auto &ar_member = _ar_members.emplace_back();
      ar_member.Buf = std::make_unique<llvm::SmallVectorMemoryBuffer>(
          std::move(content), false);
      ar_member.MemberName = obj.name;
    }

    return llvm::Error::success();
  }

  /// \brief Common config.
  llvm::objcopy::CommonConfig _common_config;

  /// \brief ELF config (empty).
  llvm::objcopy::ELFConfig _elf_config;

  /// \brief COFF config (empty).
  llvm::objcopy::COFFConfig _coff_config;

  /// \brief MachO config (empty).
  llvm::objcopy::MachOConfig _macho_config;

  /// \brief Wasm config (empty).
  llvm::objcopy::WasmConfig _wasm_config;

  /// \brief XCOFF config (empty).
  llvm::objcopy::XCOFFConfig _xcoff_config;

  /// \brief Archive members.
  llvm::SmallVector<llvm::NewArchiveMember, 128> _ar_members;

  /// \brief Bartleby handle.
  Bartleby _b;
};

BARTLEBY_API llvm::Error
Bartleby::BuildFinalArchive(Bartleby &&b,
                            llvm::StringRef out_filepath) noexcept {
  ArchiveBuilder builder(std::move(b));
  return builder.Build(out_filepath);
}

BARTLEBY_API llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
Bartleby::BuildFinalArchive(Bartleby &&b) noexcept {
  ArchiveBuilder builder(std::move(b));
  return builder.Build();
}

} // end namespace saq::bartleby
