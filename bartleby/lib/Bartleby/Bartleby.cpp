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
/// \param Sym Symbol.
///
/// \returns Pieces of information about the given symbol.
[[nodiscard]] SymbolInfo
getSymbolInfo(const llvm::object::SymbolRef &Sym) noexcept {
  SymbolInfo Info{.Sym = Sym};

  if (auto Err = Sym.getName().moveInto(Info.Name)) {
    LLVM_DEBUG(llvm::dbgs() << "failed to get name: " << Err << '\n');
    Info.Err = true;
  }
  if (auto Err = Sym.getFlags().moveInto(Info.Flags)) {
    LLVM_DEBUG(llvm::dbgs() << "failed to get flags: " << Err << '\n');
    Info.Err = true;
  }
  if (auto Err = Sym.getType().moveInto(Info.Type)) {
    LLVM_DEBUG(llvm::dbgs() << "failed to get type: " << Err << '\n');
    Info.Err = true;
  }
  return Info;
}

/// \brief Collects all the information of all symbols from an object.
///
/// \param Obj Object.
/// \param[out] SymInfos Container where to store the symbol infos.
void collectSymbolInfos(const llvm::object::ObjectFile *Obj,
                        llvm::SmallVectorImpl<SymbolInfo> &SymInfos) noexcept {
  for (const auto &Sym : Obj->symbols()) {
    auto &Info = SymInfos.emplace_back(getSymbolInfo(Sym));
    Info.ObjectType = Obj->getTripleObjectFormat();
  }
}

/// \brief Determines if we should skip a symbol based on its information.
///
/// \param SymInfo Symbol information.
///
/// \returns True if we should skip it, else false.
[[nodiscard]] bool shouldSkipSymbol(const SymbolInfo &SymInfo) noexcept {
  if (SymInfo.Err) {
    LLVM_DEBUG(llvm::dbgs()
               << "failed to get all info for symbol. Skipping it\n");
    return true;
  }

  switch (*SymInfo.Type) {
  case llvm::object::SymbolRef::Type::ST_Other:
  case llvm::object::SymbolRef::Type::ST_Debug:
  case llvm::object::SymbolRef::Type::ST_File: {
    LLVM_DEBUG(llvm::dbgs()
               << "skipping " << SymInfo.Name.value().data()
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
/// \param Object The object file.
/// \param[out] Symbols Symbol map to update.
void ProcessObjectFile(const llvm::object::ObjectFile *Object,
                       Bartleby::SymbolMap &Symbols) {
  llvm::SmallVector<SymbolInfo, 128> SymInfos;
  collectSymbolInfos(Object, SymInfos);
  for (const auto &SymInfo : SymInfos) {
    if (shouldSkipSymbol(SymInfo)) {
      continue;
    }

    std::string Name(*SymInfo.Name);

    LLVM_DEBUG(llvm::dbgs()
               << "Found symbol '" << Name << "', type: " << *SymInfo.Type
               << ", flags: " << *SymInfo.Flags << '\n');
    auto &Sym = Symbols[Name];
    Sym.updateWithNewSymbolInfo(SymInfo);
  }
}

} // end anonymous namespace

ObjectFormat::ObjectFormat(const llvm::Triple &Triple) noexcept
    : Arch(Triple.getArch()), SubArch(Triple.getSubArch()),
      FormatType(Triple.getObjectFormat()) {}

uint64_t ObjectFormat::pack() const noexcept {
  return (static_cast<uint64_t>(Arch)) |
         (static_cast<uint64_t>(SubArch) << 16) |
         (static_cast<uint64_t>(FormatType) << 32);
}

bool ObjectFormat::operator==(const ObjectFormat &Other) const noexcept {
  return pack() == Other.pack();
}

bool ObjectFormat::matches(const llvm::Triple &Triple) const noexcept {
  return ObjectFormat{Triple}.pack() == pack();
}

llvm::raw_ostream &
saq::bartleby::operator<<(llvm::raw_ostream &OS,
                          const ObjectFormat &ObjFormat) noexcept {
  return OS << "ObjectFormat(arch=" << ObjFormat.Arch
            << ", subarch=" << ObjFormat.SubArch
            << ", file format=" << ObjFormat.FormatType << ')';
}

BARTLEBY_API llvm::Error Bartleby::addBinary(
    llvm::object::OwningBinary<llvm::object::Binary> OwningBinary) noexcept {
  auto *Binary = OwningBinary.getBinary();
  llvm::Error E = llvm::Error::success();

  if (auto *Obj = llvm::dyn_cast<llvm::object::ObjectFile>(Binary)) {
    const auto Triple = Obj->makeTriple();
    if (!objectFormatMatches(Triple)) {
      return llvm::make_error<Error>(Error::ObjectFormatTypeMismatchReason{
          .Constraint = std::get<ObjectFormat>(ObjFormat), .Found = {Triple}});
    }
    ObjFormat = Triple;
    ProcessObjectFile(Obj, Symbols);
    auto &Entry = Objects.emplace_back(ObjectFile{.Handle = Obj});
    (llvm::Twine(llvm::utostr(Objects.size())) + ".o")
        .toNullTerminatedStringRef(Entry.Name);
  } else if (auto *Archive = llvm::dyn_cast<llvm::object::Archive>(Binary)) {
    llvm::Error E = llvm::Error::success();
    for (const auto &Ch : Archive->children(E)) {
      auto BinOrErr = Ch.getAsBinary();
      if (!BinOrErr) {
        return BinOrErr.takeError();
      }
      if (auto *Obj =
              llvm::dyn_cast<llvm::object::ObjectFile>(BinOrErr.get().get())) {
        const auto Triple = Obj->makeTriple();
        if (!objectFormatMatches(Triple)) {
          return llvm::make_error<Error>(Error::ObjectFormatTypeMismatchReason{
              .Constraint = std::get<ObjectFormat>(ObjFormat),
              .Found = {Triple}});
        }
        ObjFormat = Triple;

        ProcessObjectFile(Obj, Symbols);
        auto &Entry = Objects.emplace_back(ObjectFile{
            .Owner = std::move(BinOrErr.get()),
        });
        Entry.Handle = Obj;
        if (auto NameOrErr = Ch.getName()) {
          Entry.Name = *NameOrErr;
        } else {
          (llvm::Twine(llvm::utostr(Objects.size())) + ".o")
              .toNullTerminatedStringRef(Entry.Name);
        }
      } else {
        Error::UnsupportedBinaryReason Reason;
        llvm::raw_svector_ostream OS(Reason.Msg);
        OS << "unsupported binary '" << Binary->getType()
           << "' (triple: " << Binary->getTripleObjectFormat() << ')';
        return llvm::make_error<Error>(std::move(Reason));
      }
    }
  } else if (Binary->isMachOUniversalBinary()) {
    return addMachOUniversalBinary(std::move(OwningBinary));
  } else {
    Error::UnsupportedBinaryReason Reason;
    llvm::raw_svector_ostream OS(Reason.Msg);
    OS << "unsupported binary '" << Binary->getType()
       << "' (triple: " << Binary->getTripleObjectFormat() << ')';
    return llvm::make_error<Error>(std::move(Reason));
  }
  OwnedBinaries.push_back(std::move(OwningBinary));
  return llvm::Error::success();
}

BARTLEBY_API size_t
Bartleby::prefixGlobalAndDefinedSymbols(llvm::StringRef Prefix) noexcept {
  size_t N = 0;
  const auto End = Symbols.end();
  for (auto Entry = Symbols.begin(); Entry != End; ++Entry) {
    const auto &Name = Entry->first();
    auto &Sym = Entry->getValue();
    if (Sym.isGlobal() && Sym.isDefined()) {
      std::string NewName;
      if (Sym.isMachO()) {
        NewName += '_';
      }
      NewName += Prefix;

      if (Sym.isMachO()) {
        NewName += Name.substr(1);
      } else {
        NewName += Name;
      }
      Sym.setName(std::move(NewName));
      ++N;
    }
  }

  return N;
}

bool Bartleby::objectFormatMatches(const ObjectFormat &ObjFmt) const noexcept {
  if (const auto *F = std::get_if<ObjectFormat>(&ObjFormat)) {
    return *F == ObjFmt;
  }
  return std::holds_alternative<std::monostate>(ObjFormat);
}

bool Bartleby::isMachOUniversalBinary() const noexcept {
  return std::holds_alternative<ObjectFormatSet>(ObjFormat);
}

llvm::Error Bartleby::addMachOUniversalBinary(
    llvm::object::OwningBinary<llvm::object::Binary> OwningBinary) noexcept {
  auto *Fat = llvm::dyn_cast<llvm::object::MachOUniversalBinary>(
      OwningBinary.getBinary());
  assert(Fat != nullptr);

  if (const auto *Type = std::get_if<ObjectFormat>(&ObjFormat)) {
    Error::MachOUniversalBinaryReason Reason;
    llvm::raw_svector_ostream OS(Reason.Msg);
    OS << "expected an object of type " << *Type << ", got a fat Mach-O";
    return llvm::make_error<Error>(std::move(Reason));
  }

  auto *Formats = std::get_if<ObjectFormatSet>(&ObjFormat);
  if ((Formats != nullptr) && (Formats->size() != Fat->getNumberOfObjects())) {
    Error::MachOUniversalBinaryReason Reason;
    llvm::raw_svector_ostream OS(Reason.Msg);
    OS << "expected a fat Mach-O with " << Formats->size() << " arch(s), got "
       << Fat->getNumberOfObjects() << " arch(s).";
    return llvm::make_error<Error>(std::move(Reason));
  }

  if (Formats == nullptr) {
    ObjFormat = ObjectFormatSet();
    Formats = std::get_if<ObjectFormatSet>(&ObjFormat);
    for (const auto &Ofa : Fat->objects()) {
      Formats->insert(Ofa.getTriple());
    }
  }

  for (auto &Ofa : Fat->objects()) {
    const auto Triple = Ofa.getTriple();
    if (Formats->count(Triple) == 0) {
      Error::MachOUniversalBinaryReason Reason;
      llvm::raw_svector_ostream OS(Reason.Msg);
      OS << "unexpected triple " << Ofa.getTriple().str() << " in fat Mach-O";
      return llvm::make_error<Error>(std::move(Reason));
    }

    if (auto ObjOrErr = Ofa.getAsObjectFile()) {
      auto Obj = std::move(*ObjOrErr);
      ProcessObjectFile(&*Obj, Symbols);
      auto &Entry = Objects.emplace_back(ObjectFile{
          .Handle = &*Obj,
          .Owner = std::move(Obj),
          .Alignment = Ofa.getAlign(),
      });
      (llvm::Twine(llvm::utostr(Objects.size())) + ".o")
          .toNullTerminatedStringRef(Entry.Name);
      continue;
    }

    if (auto ArOrErr = Ofa.getAsArchive()) {
      auto Ar = std::move(*ArOrErr);
      llvm::Error E = llvm::Error::success();
      for (const auto &Ch : Ar->children(E)) {
        auto BinOrErr = Ch.getAsBinary();
        if (!BinOrErr) {
          return BinOrErr.takeError();
        }
        auto Bin = std::move(*BinOrErr);

        if (auto *Obj = llvm::dyn_cast<llvm::object::MachOObjectFile>(&*Bin)) {
          ProcessObjectFile(Obj, Symbols);
          auto &Entry = Objects.emplace_back(ObjectFile{
              .Handle = &*Obj,
              .Owner = std::move(Bin),
              .Alignment = 0,
          });
          if (auto NameOrErr = Ch.getName()) {
            Entry.Name = *NameOrErr;
          } else {
            (llvm::Twine(llvm::utostr(Objects.size())) + ".o")
                .toNullTerminatedStringRef(Entry.Name);
          }
          continue;
        }
        Error::MachOUniversalBinaryReason Reason;
        llvm::raw_svector_ostream OS(Reason.Msg);
        OS << "expected an object in the archive, found " << Bin->getType();
        return llvm::make_error<Error>(std::move(Reason));
      }
    }
  }
  OwnedBinaries.push_back(std::move(OwningBinary));

  return llvm::Error::success();
}
