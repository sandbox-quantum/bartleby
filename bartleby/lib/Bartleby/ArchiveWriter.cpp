///
/// \file
/// \brief Archive writer implementation.
///
/// \author thb-sb

#include "llvm/Object/ArchiveWriter.h"
#include "llvm/ObjCopy/COFF/COFFConfig.h"
#include "llvm/ObjCopy/CommonConfig.h"
#include "llvm/ObjCopy/ELF/ELFConfig.h"
#include "llvm/ObjCopy/MachO/MachOConfig.h"
#include "llvm/ObjCopy/MultiFormatConfig.h"
#include "llvm/ObjCopy/ObjCopy.h"
#include "llvm/ObjCopy/XCOFF/XCOFFConfig.h"
#include "llvm/ObjCopy/wasm/WasmConfig.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/MachOUniversalWriter.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"

#include "Bartleby/Bartleby.h"

#include "Bartleby/Error.h"
#include "Bartleby/Export.h"

#define DEBUG_TYPE "Bartleby"

namespace saq::bartleby {

namespace {

/// \brief An archive to build.
struct Archive {
  /// \brief Archive members.
  llvm::SmallVector<llvm::NewArchiveMember, 128> members;

  /// \brief The buffer which will contain the archive.
  std::unique_ptr<llvm::MemoryBuffer> out_buffer;

  /// \brief The final archive properties.
  std::unique_ptr<llvm::object::Archive> out_ar;

  /// \brief Archive triple.
  llvm::Triple triple;

  /// \brief Name.
  std::unique_ptr<std::string> name;

  /// \brief Alignment.
  uint32_t alignment;
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
  /// \brief Constructor using a Bartleby handle.
  ///
  /// \param b Bartleby handle.
  ArchiveWriter(Bartleby &&b) noexcept : _b{std::move(b)} {
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

  /// \brief Build the slices needed for a fat Mach-O.
  ///
  /// \param slices Vector where to store slices.
  /// \param archives Vector where to write archives.
  ///
  /// \return An error.
  [[nodiscard]] llvm::Error BuildMachOUniversalBinarySlices(
      llvm::SmallVectorImpl<llvm::object::Slice> &slices,
      ArchiveMap &archives) noexcept {
    assert(_b.isMachOUniversalBinary());

    const auto &object_format_set =
        std::get<ObjectFormatSet>(_b._object_format);

    LLVM_DEBUG(for (const auto &object_format
                    : object_format_set) {
      llvm::dbgs() << "object format in fat Mach-O: " << object_format << '\n';
    });

    archives.reserve(object_format_set.size());
    slices.reserve(object_format_set.size());

    for (const auto &obj : _b._objects) {
      const auto triple = obj.handle->makeTriple();
      LLVM_DEBUG(llvm::dbgs()
                 << "got object, triple is " << triple.str()
                 << ", object format is " << ObjectFormat{triple} << '\n');
      assert(object_format_set.count(triple) == 1);
      auto final_obj_or_err = ExecuteObjCopyOnObject(obj);
      if (!final_obj_or_err) {
        return final_obj_or_err.takeError();
      }
      auto &ar = archives[ObjectFormat{triple}];
      ar.triple = triple;
      ar.alignment = obj.alignment;
      auto &ar_member = ar.members.emplace_back();
      ar_member.Buf = std::move(final_obj_or_err.get());
      ar.name = std::make_unique<std::string>(triple.str());
      ar_member.MemberName = *ar.name;
    }

    for (auto &[object_format, ar] : archives) {
      if (auto buffer_or_err = llvm::writeArchiveToBuffer(
              ar.members,
              /* WriteSymtab= */ true, ar.members[0].detectKindFromObject(),
              /* Deterministic=*/true, /*Thin=*/false)) {
        ar.out_buffer = std::move(buffer_or_err.get());
      } else {
        return buffer_or_err.takeError();
      }

      auto new_ar_or_err = llvm::object::Archive::create(*ar.out_buffer);
      if (!new_ar_or_err) {
        return new_ar_or_err.takeError();
      }

      auto cpu_type_or_err = llvm::MachO::getCPUType(ar.triple);
      if (!cpu_type_or_err) {
        return cpu_type_or_err.takeError();
      }

      auto cpu_sub_type_or_err = llvm::MachO::getCPUSubType(ar.triple);
      if (!cpu_sub_type_or_err) {
        return cpu_sub_type_or_err.takeError();
      }
      ar.out_ar = std::move(new_ar_or_err.get());
      slices.emplace_back(*ar.out_ar, cpu_type_or_err.get(),
                          cpu_sub_type_or_err.get(), ar.triple.str(),
                          ar.alignment);
    }

    return llvm::Error::success();
  }

  /// \brief Build a fat Mach-O file.
  ///
  /// \param out_filepath Path to out file.
  ///
  /// \return An error.
  [[nodiscard]] llvm::Error
  BuildMachOUniversalBinary(llvm::StringRef out_filepath) noexcept {
    llvm::SmallVector<llvm::object::Slice, 3> slices;
    ArchiveMap archives;

    if (auto err = BuildMachOUniversalBinarySlices(slices, archives)) {
      return err;
    }

    return llvm::object::writeUniversalBinary(slices, out_filepath);
  }

  /// \brief Build a fat Mach-O file.
  ///
  /// \return A memory buffer, or an error..
  [[nodiscard]] llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  BuildMachOUniversalBinary() noexcept {
    llvm::SmallVector<llvm::object::Slice, 3> slices;
    ArchiveMap archives;

    if (auto err = BuildMachOUniversalBinarySlices(slices, archives)) {
      return err;
    }

    llvm::SmallVector<char, 8192> content;
    llvm::raw_svector_ostream os(content);

    if (auto err = llvm::object::writeUniversalBinaryToStream(slices, os)) {
      return err;
    }

    return std::make_unique<llvm::SmallVectorMemoryBuffer>(std::move(content),
                                                           false);
  }

  /// \brief Build the final archive.
  ///
  /// \param out_filepath Path to out file.
  ///
  /// \return An error.
  [[nodiscard]] llvm::Error Build(llvm::StringRef out_filepath) noexcept {
    if (_b.isMachOUniversalBinary()) {
      return BuildMachOUniversalBinary(out_filepath);
    }

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
    if (_b.isMachOUniversalBinary()) {
      return BuildMachOUniversalBinary();
    }

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
  ~ArchiveWriter() noexcept override = default;

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
  /// \brief Execute objcopy on an object.
  ///
  /// \param obj The object.
  ///
  /// \return The content of the final object, or an error.
  [[nodiscard]] llvm::Expected<std::unique_ptr<llvm::SmallVectorMemoryBuffer>>
  ExecuteObjCopyOnObject(const ObjectFile &obj) noexcept {
    llvm::SmallVector<char, 8192> content;
    llvm::raw_svector_ostream os(content);
    if (auto err =
            llvm::objcopy::executeObjcopyOnBinary(*this, *obj.handle, os)) {
      return err;
    }
    return std::make_unique<llvm::SmallVectorMemoryBuffer>(std::move(content),
                                                           false);
  }

  /// \brief Execute objcopy on objects that belongs to the Bartleby handle.
  ///
  /// \return An error.
  [[nodiscard]] llvm::Error ExecuteObjCopyOnObjects() noexcept {
    LLVM_DEBUG(llvm::dbgs()
               << "processing " << _b._objects.size() << " object(s)\n");
    for (auto &obj : _b._objects) {
      if (auto final_obj_or_err = ExecuteObjCopyOnObject(obj)) {
        auto &ar_member = _ar_members.emplace_back();
        ar_member.Buf = std::move(final_obj_or_err.get());
        ar_member.MemberName = obj.name;
      } else {
        return final_obj_or_err.takeError();
      }
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
  ArchiveWriter builder(std::move(b));
  return builder.Build(out_filepath);
}

BARTLEBY_API llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
Bartleby::BuildFinalArchive(Bartleby &&b) noexcept {
  ArchiveWriter builder(std::move(b));
  return builder.Build();
}

} // end namespace saq::bartleby