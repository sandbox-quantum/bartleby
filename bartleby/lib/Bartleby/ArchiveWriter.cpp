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
#include "llvm/Support/SmallVectorMemoryBuffer.h"

#include "Bartleby/Bartleby.h"

#include "Bartleby/Error.h"
#include "Bartleby/Export.h"

#define DEBUG_TYPE "Bartleby"

namespace saq::bartleby {

/// \brief Archive builder that implements out multi format config.
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
  ArchiveWriter builder(std::move(b));
  return builder.Build(out_filepath);
}

BARTLEBY_API llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
Bartleby::BuildFinalArchive(Bartleby &&b) noexcept {
  ArchiveWriter builder(std::move(b));
  return builder.Build();
}

} // end namespace saq::bartleby