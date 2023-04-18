///
/// \file
/// \brief Archive writer implementation.
///
/// \author thb-sb

#include "Bartleby/Bartleby.h"
#include "Bartleby/Error.h"
#include "Bartleby/Export.h"

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
#include "llvm/Object/MachOUniversalWriter.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"

#define DEBUG_TYPE "Bartleby"

using namespace saq::bartleby;

namespace {

/// \brief An archive to build.
struct Archive {
  /// \brief Archive members.
  llvm::SmallVector<llvm::NewArchiveMember, 128> Members;

  /// \brief The buffer which will contain the archive.
  std::unique_ptr<llvm::MemoryBuffer> OutBuffer;

  /// \brief The final archive properties.
  std::unique_ptr<llvm::object::Archive> OutArchive;

  /// \brief Archive triple.
  llvm::Triple Triple;

  /// \brief Name.
  std::unique_ptr<std::string> Name;

  /// \brief Alignment.
  uint32_t Alignment;
};

/// \brief A map that links an ObjectFormat with an Archive.
using ArchiveMap =
    std::unordered_map<ObjectFormat, Archive, ObjectFormat::Hash>;

} // end anonymous namespace

/// \brief Archive builder that implements our multi format config.
///
/// See https://llvm.org/doxygen/classllvm_1_1objcopy_1_1MultiFormatConfig.html.
class Bartleby::ArchiveWriter : public llvm::objcopy::MultiFormatConfig {
public:
  /// \brief Constructs an \p ArchiveWriter using a Bartleby handle.
  ///
  /// \param[in] B Bartleby handle.
  ArchiveWriter(Bartleby &&B) noexcept : Handle(std::move(B)) {
    const auto End = Handle.Symbols.end();
    for (auto Entry = Handle.Symbols.begin(); Entry != End; ++Entry) {
      const auto &Name = Entry->first();
      const auto &Sym = Entry->getValue();
      if (const auto OName = Sym.getOverwriteName(); OName) {
        LLVM_DEBUG(llvm::dbgs() << "bartleby is going to rename '" << Name
                                << "' into '" << *OName << "'\n");
        CommonConfig.SymbolsToRename[Name] = *OName;
      }
    }
  }

  /// \brief Constructs the slices needed for a fat Mach-O.
  ///
  /// \param[out] Slices Vector where to store slices.
  /// \param[out] Archives Vector where to write archives.
  ///
  /// \returns An error.
  [[nodiscard]] llvm::Error buildMachOUniversalBinarySlices(
      llvm::SmallVectorImpl<llvm::object::Slice> &Slices,
      ArchiveMap &Archives) noexcept {
    assert(Handle.isMachOUniversalBinary());

    const auto &ObjFmtSet = std::get<ObjectFormatSet>(Handle.ObjFormat);

    LLVM_DEBUG(for (const auto &Fmt
                    : ObjFmtSet) {
      llvm::dbgs() << "object format in fat Mach-O: " << Fmt << '\n';
    });

    Archives.reserve(ObjFmtSet.size());
    Slices.reserve(ObjFmtSet.size());

    for (const auto &Obj : Handle.Objects) {
      const auto Triple = Obj.Handle->makeTriple();
      LLVM_DEBUG(llvm::dbgs()
                 << "got object, triple is " << Triple.str()
                 << ", object format is " << ObjectFormat{Triple} << '\n');
      assert(ObjFmtSet.count(Triple) == 1);
      auto FinalObjOrErr = executeObjCopyOnObject(Obj);
      if (!FinalObjOrErr) {
        return FinalObjOrErr.takeError();
      }
      auto &Ar = Archives[ObjectFormat{Triple}];
      Ar.Triple = Triple;
      Ar.Alignment = Obj.Alignment;
      auto &ArMember = Ar.Members.emplace_back();
      ArMember.Buf = std::move(*FinalObjOrErr);
      Ar.Name = std::make_unique<std::string>(Triple.str());
      ArMember.MemberName = *Ar.Name;
    }

    for (auto &[Fmt, Ar] : Archives) {
      if (auto BufferOrErr = llvm::writeArchiveToBuffer(
              Ar.Members,
              /* WriteSymtab= */ true, Ar.Members[0].detectKindFromObject(),
              /* Deterministic=*/true, /*Thin=*/false)) {
        Ar.OutBuffer = std::move(*BufferOrErr);
      } else {
        return BufferOrErr.takeError();
      }

      auto NewArOrErr = llvm::object::Archive::create(*Ar.OutBuffer);
      if (!NewArOrErr) {
        return NewArOrErr.takeError();
      }

      auto CPUTypeOrErr = llvm::MachO::getCPUType(Ar.Triple);
      if (!CPUTypeOrErr) {
        return CPUTypeOrErr.takeError();
      }

      auto CPUSubTypeOrErr = llvm::MachO::getCPUSubType(Ar.Triple);
      if (!CPUSubTypeOrErr) {
        return CPUSubTypeOrErr.takeError();
      }
      Ar.OutArchive = std::move(*NewArOrErr);
      Slices.emplace_back(*Ar.OutArchive, *CPUTypeOrErr, *CPUSubTypeOrErr,
                          Ar.Triple.str(), Ar.Alignment);
    }

    return llvm::Error::success();
  }

  /// \brief Builds a fat Mach-O file and writes the output to a file.
  ///
  /// \param OutFilepath Path to out file.
  ///
  /// \returns An error.
  [[nodiscard]] llvm::Error
  buildMachOUniversalBinary(llvm::StringRef OutFilepath) noexcept {
    llvm::SmallVector<llvm::object::Slice, 3> Slices;
    ArchiveMap Archives;

    if (auto Err = buildMachOUniversalBinarySlices(Slices, Archives)) {
      return Err;
    }

    return llvm::object::writeUniversalBinary(Slices, OutFilepath);
  }

  /// \brief Builds a fat Mach-O file and returns its content.
  ///
  /// \returns A memory buffer, or an error.
  [[nodiscard]] llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  buildMachOUniversalBinary() noexcept {
    llvm::SmallVector<llvm::object::Slice, 3> Slices;
    ArchiveMap Archives;

    if (auto Err = buildMachOUniversalBinarySlices(Slices, Archives)) {
      return Err;
    }

    llvm::SmallVector<char, 8192> Content;
    llvm::raw_svector_ostream OS(Content);

    if (auto Err = llvm::object::writeUniversalBinaryToStream(Slices, OS)) {
      return Err;
    }

    return std::make_unique<llvm::SmallVectorMemoryBuffer>(std::move(Content),
                                                           false);
  }

  /// \brief Builds the final archive and writes the content to a file.
  ///
  /// \param OutFilepath Path to out file.
  ///
  /// \returns An error.
  [[nodiscard]] llvm::Error build(llvm::StringRef OutFilepath) noexcept {
    if (Handle.isMachOUniversalBinary()) {
      return buildMachOUniversalBinary(OutFilepath);
    }

    if (auto Err = executeObjCopyOnObjects()) {
      return Err;
    }

    return llvm::writeArchive(OutFilepath, ArMembers,
                              /* WriteSymtab= */ true,
                              ArMembers[0].detectKindFromObject(),
                              /* Deterministic= */ true,
                              /* Thin= */ false);
  }

  /// \brief Builds the final archive and returns its content.
  ///
  /// \returns A memory buffer or an error.
  [[nodiscard]] llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  build() noexcept {
    if (Handle.isMachOUniversalBinary()) {
      return buildMachOUniversalBinary();
    }

    if (auto Err = executeObjCopyOnObjects()) {
      return Err;
    }

    return llvm::writeArchiveToBuffer(ArMembers,
                                      /* WriteSymtab= */ true,
                                      ArMembers[0].detectKindFromObject(),
                                      /* Deterministic= */ true,
                                      /* Thin= */ false);
  }

  ~ArchiveWriter() noexcept override = default;

  const llvm::objcopy::CommonConfig &getCommonConfig() const noexcept override {
    return CommonConfig;
  }

  llvm::Expected<const llvm::objcopy::ELFConfig &>
  getELFConfig() const noexcept override {
    return ELFConfig;
  }

  llvm::Expected<const llvm::objcopy::COFFConfig &>
  getCOFFConfig() const noexcept override {
    return COFFConfig;
  }

  llvm::Expected<const llvm::objcopy::MachOConfig &>
  getMachOConfig() const noexcept override {
    return MachOConfig;
  }

  llvm::Expected<const llvm::objcopy::WasmConfig &>
  getWasmConfig() const noexcept override {
    return WasmConfig;
  }

  llvm::Expected<const llvm::objcopy::XCOFFConfig &>
  getXCOFFConfig() const noexcept override {
    return XCOFFConfig;
  }

private:
  /// \brief Executes \p objcopy on an object.
  ///
  /// \param Obj The object.
  ///
  /// \returns The content of the final object, or an error.
  [[nodiscard]] llvm::Expected<std::unique_ptr<llvm::SmallVectorMemoryBuffer>>
  executeObjCopyOnObject(const ObjectFile &Obj) noexcept {
    llvm::SmallVector<char, 8192> Content;
    llvm::raw_svector_ostream OS(Content);
    if (auto Err =
            llvm::objcopy::executeObjcopyOnBinary(*this, *Obj.Handle, OS)) {
      return Err;
    }
    return std::make_unique<llvm::SmallVectorMemoryBuffer>(std::move(Content),
                                                           false);
  }

  /// \brief Executes \p objcopy on objects belonging to the Bartleby handle.
  ///
  /// \returns An error.
  [[nodiscard]] llvm::Error executeObjCopyOnObjects() noexcept {
    LLVM_DEBUG(llvm::dbgs()
               << "processing " << Handle.Objects.size() << " object(s)\n");
    for (auto &Obj : Handle.Objects) {
      if (auto FinalObjOrErr = executeObjCopyOnObject(Obj)) {
        auto &ArMember = ArMembers.emplace_back();
        ArMember.Buf = std::move(*FinalObjOrErr);
        ArMember.MemberName = Obj.Name;
      } else {
        return FinalObjOrErr.takeError();
      }
    }

    return llvm::Error::success();
  }

  /// \brief Common config.
  llvm::objcopy::CommonConfig CommonConfig;

  /// \brief ELF config (empty).
  llvm::objcopy::ELFConfig ELFConfig;

  /// \brief COFF config (empty).
  llvm::objcopy::COFFConfig COFFConfig;

  /// \brief MachO config (empty).
  llvm::objcopy::MachOConfig MachOConfig;

  /// \brief Wasm config (empty).
  llvm::objcopy::WasmConfig WasmConfig;

  /// \brief XCOFF config (empty).
  llvm::objcopy::XCOFFConfig XCOFFConfig;

  /// \brief Archive members.
  llvm::SmallVector<llvm::NewArchiveMember, 128> ArMembers;

  /// \brief Bartleby handle.
  Bartleby Handle;
};

BARTLEBY_API llvm::Error
Bartleby::buildFinalArchive(Bartleby &&B,
                            llvm::StringRef OutFilepath) noexcept {
  ArchiveWriter Builder(std::move(B));
  return Builder.build(OutFilepath);
}

BARTLEBY_API llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
Bartleby::buildFinalArchive(Bartleby &&B) noexcept {
  ArchiveWriter Builder(std::move(B));
  return Builder.build();
}
