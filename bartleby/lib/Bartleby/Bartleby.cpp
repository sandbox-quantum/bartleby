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

#include "Bartleby/Error.h"
#include "Bartleby/Export.h"
#include "Bartleby/Symbol.h"

#include "llvm/Object/Archive.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Support/MemoryBuffer.h"

#define DEBUG_TYPE "bartleby"

using namespace saq::bartleby;

BARTLEBY_API Bartleby::Bartleby() noexcept = default;

namespace {

/// \brief Fetches various information from a symbol.
///
/// \param sym Symbol.
///
/// \returns Pieces of information about the given symbol.
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

/// \brief Collects all the information of all symbols from an object.
///
/// \param obj Object.
/// \param[out] syminfos Container where to store the symbol infos.
void collectSymbolInfos(const llvm::object::ObjectFile *obj,
                        llvm::SmallVectorImpl<SymbolInfo> &syminfos) noexcept {
  for (const auto &sym : obj->symbols()) {
    auto &info = syminfos.emplace_back(getSymbolInfo(sym));
    info.object_type = obj->getTripleObjectFormat();
  }
}

/// \brief Determines if we should skip a symbol based on its information.
///
/// \param syminfo Symbol information.
///
/// \returns True if we should skip it, else false.
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

/// \brief Processes an object file.
///
/// \param object The object file.
/// \param[out] symbols Symbol map to update.
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

llvm::raw_ostream &
saq::bartleby::operator<<(llvm::raw_ostream &os,
                          const ObjectFormat &object_format) noexcept {
  return os << "ObjectFormat(arch=" << object_format.arch
            << ", subarch=" << object_format.subarch
            << ", file format=" << object_format.format_type << ')';
}

BARTLEBY_API llvm::Error Bartleby::AddBinary(
    llvm::object::OwningBinary<llvm::object::Binary> owning_binary) noexcept {
  auto *binary = owning_binary.getBinary();
  llvm::Error e = llvm::Error::success();

  if (auto *obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary)) {
    const auto triple = obj->makeTriple();
    if (!objectFormatMatches(triple)) {
      return llvm::make_error<Error>(Error::ObjectFormatTypeMismatchReason{
          .constraint = std::get<ObjectFormat>(_object_format),
          .found = {triple}});
    }
    _object_format = triple;
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
        const auto triple = obj->makeTriple();
        if (!objectFormatMatches(triple)) {
          return llvm::make_error<Error>(Error::ObjectFormatTypeMismatchReason{
              .constraint = std::get<ObjectFormat>(_object_format),
              .found = {triple}});
        }
        _object_format = triple;

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
  } else if (binary->isMachOUniversalBinary()) {
    return AddMachOUniversalBinary(std::move(owning_binary));
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

bool Bartleby::objectFormatMatches(
    const ObjectFormat &object_format) const noexcept {
  if (const auto *f = std::get_if<ObjectFormat>(&_object_format)) {
    return *f == object_format;
  }
  return std::holds_alternative<std::monostate>(_object_format);
}

bool Bartleby::isMachOUniversalBinary() const noexcept {
  return std::holds_alternative<ObjectFormatSet>(_object_format);
}

llvm::Error Bartleby::AddMachOUniversalBinary(
    llvm::object::OwningBinary<llvm::object::Binary> owning_binary) noexcept {
  auto *fat = llvm::dyn_cast<llvm::object::MachOUniversalBinary>(
      owning_binary.getBinary());
  assert(fat != nullptr);

  if (const auto *type = std::get_if<ObjectFormat>(&_object_format)) {
    Error::MachOUniversalBinaryReason reason;
    llvm::raw_svector_ostream os(reason.msg);
    os << "expected an object of type " << *type << ", got a fat Mach-O";
    return llvm::make_error<Error>(std::move(reason));
  }

  auto *formats = std::get_if<ObjectFormatSet>(&_object_format);
  if ((formats != nullptr) && (formats->size() != fat->getNumberOfObjects())) {
    Error::MachOUniversalBinaryReason reason;
    llvm::raw_svector_ostream os(reason.msg);
    os << "expected a fat Mach-O with " << formats->size() << " arch(s), got "
       << fat->getNumberOfObjects() << " arch(s).";
    return llvm::make_error<Error>(std::move(reason));
  }

  if (formats == nullptr) {
    _object_format = ObjectFormatSet();
    formats = std::get_if<ObjectFormatSet>(&_object_format);
    for (const auto &ofa : fat->objects()) {
      formats->insert(ofa.getTriple());
    }
  }

  for (auto &ofa : fat->objects()) {
    const auto triple = ofa.getTriple();
    if (formats->count(triple) == 0) {
      Error::MachOUniversalBinaryReason reason;
      llvm::raw_svector_ostream os(reason.msg);
      os << "unexpected triple " << ofa.getTriple().str() << " in fat Mach-O";
      return llvm::make_error<Error>(std::move(reason));
    }

    if (auto obj_or_err = ofa.getAsObjectFile()) {
      auto obj = std::move(obj_or_err.get());
      ProcessObjectFile(&*obj, _symbols);
      auto &entry = _objects.emplace_back(ObjectFile{
          .handle = &*obj,
          .owner = std::move(obj),
          .alignment = ofa.getAlign(),
      });
      (llvm::Twine(llvm::utostr(_objects.size())) + ".o")
          .toNullTerminatedStringRef(entry.name);
    }
  }
  _owned_binaries.push_back(std::move(owning_binary));

  return llvm::Error::success();
}
