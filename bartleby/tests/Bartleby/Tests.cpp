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
/// \brief Unit tests for Bartleby.
///
/// \author thb-sb

#include "Bartleby/Bartleby.h"

#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Object/Binary.h"
#include "llvm/ObjectYAML/yaml2obj.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"

#include <unistd.h>

#include "gtest/gtest.h"

namespace {

/// \brief Base directory for test data based on yaml files.
#define TEST_DATA_BASE_DIR "bartleby/tests/Bartleby/testdata/yaml"

/// \brief Converts a \p llvm::Triple::ObjectFormatType into a path to a
/// sub-directory of \p TEST_DATA_BASE_DIR.
///
/// \param Format Object format.
///
/// \returns The path to the directory containing yaml files for the format.
[[nodiscard]] llvm::StringRef
objectFormatToPath(const llvm::Triple::ObjectFormatType Format) {
  switch (Format) {
  case llvm::Triple::ObjectFormatType::COFF: {
    return TEST_DATA_BASE_DIR "/COFF";
  }
  case llvm::Triple::ObjectFormatType::DXContainer: {
    return TEST_DATA_BASE_DIR "/DXContainer";
  }
  case llvm::Triple::ObjectFormatType::ELF: {
    return TEST_DATA_BASE_DIR "/ELF";
  }
  case llvm::Triple::ObjectFormatType::GOFF: {
    return TEST_DATA_BASE_DIR "/GOFF";
  }
  case llvm::Triple::ObjectFormatType::MachO: {
    return TEST_DATA_BASE_DIR "/MachO";
  }
  case llvm::Triple::ObjectFormatType::SPIRV: {
    return TEST_DATA_BASE_DIR "/SPIRV";
  }
  case llvm::Triple::ObjectFormatType::Wasm: {
    return TEST_DATA_BASE_DIR "/Wasm";
  }
  case llvm::Triple::ObjectFormatType::XCOFF: {
    return TEST_DATA_BASE_DIR "/XCOFF";
  }
  default: {
    __builtin_unreachable();
  }
  }
}

/// \brief Handles some errors that may occur when parsing a YAML document.
///
/// \param Msg The message that describes the error.
void YAMLErrorHandler(const llvm::Twine &Msg) {
  llvm::errs() << "an error occured: " << Msg << '\n';
}

/// \brief Reads a file under the directory of a specific format object.
///
/// \param Filepath Filepath.
/// \param ObjFormat Object format.
/// \param[out] Content Content of the file.
///
/// \returns An assertion result.
[[nodiscard]] testing::AssertionResult
readYAMLFile(llvm::StringRef Filepath,
             const llvm::Triple::ObjectFormatType ObjFormat,
             std::unique_ptr<llvm::MemoryBuffer> &Content) {
  if (auto ContentOrErr = llvm::MemoryBuffer::getFile(
          objectFormatToPath(ObjFormat) + "/" + Filepath, true)) {
    Content = std::move(*ContentOrErr);
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << "failed to read " << Filepath.data()
                                       << ": " << ContentOrErr.getError();
  }
}

/// \brief Parses a YAML file and fills a vector with some \p llvm::ObjectFile.
///
/// \param Filepath Filepath to the yaml file.
/// \param ObjFormat Object format.
/// \param[out] Objects Vector of objects to fill.
/// \param N Number of objects to retrieve.
///
/// \returns An assertion.
[[nodiscard]] testing::AssertionResult YAML2Objects(
    llvm::StringRef Filepath, llvm::Triple::ObjectFormatType ObjFormat,
    llvm::SmallVectorImpl<llvm::object::OwningBinary<llvm::object::Binary>>
        &Objects,
    const size_t N = 1) {
  std::unique_ptr<llvm::MemoryBuffer> Content;
  if (auto Err = readYAMLFile(Filepath, ObjFormat, Content); !Err) {
    return testing::AssertionFailure()
           << "failed to read file: " << Err.message();
  }

  for (size_t DocNum = 0; DocNum < N; ++DocNum) {
    llvm::SmallVector<char, 2048> ObjContent;
    llvm::raw_svector_ostream OS(ObjContent);

    llvm::yaml::Input YAMLIn(*Content);
    if (!llvm::yaml::convertYAML(YAMLIn, OS, YAMLErrorHandler, DocNum + 1)) {
      return testing::AssertionFailure() << "failed to convert YAML";
    }

    auto OutBuffer = std::make_unique<llvm::SmallVectorMemoryBuffer>(
        std::move(ObjContent), false);

    auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(*OutBuffer);
    if (!ObjOrErr) {
      return testing::AssertionFailure()
             << "failed to parse object: "
             << llvm::toString(ObjOrErr.takeError());
    }

    Objects.emplace_back(std::move(*ObjOrErr), std::move(OutBuffer));
  }

  return testing::AssertionSuccess();
}

/// \brief Resolves a symbol in the map of symbols.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
///
/// \returns The symbol entry.
#define ASSERT_SYM_RESOLVE(_b_, _name_)                                        \
  const auto &_s_ = (_b_).getSymbols().find((_name_));                         \
  ASSERT_NE(_s_, (_b_).getSymbols().end())                                     \
      << "Symbol " << (_name_) << " not found"

/// \brief Asserts the globalness of a symbol.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_GLOBALNESS(_b_, _name_, _exp_)                              \
  do {                                                                         \
    ASSERT_SYM_RESOLVE((_b_), (_name_));                                       \
    ASSERT_EQ(_s_->getValue().isGlobal(), (_exp_));                            \
  } while (0)

/// \brief Asserts that a symbol is local.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_LOCAL(_b_, _name_)                                          \
  ASSERT_SYM_GLOBALNESS((_b_), (_name_), false)

/// \brief Asserts that a symbol is local.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_GLOBAL(_b_, _name_)                                         \
  ASSERT_SYM_GLOBALNESS((_b_), (_name_), true)

/// \brief Asserts the definedness of a symbol.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_DEFINEDNESS(_b_, _name_, _exp_)                             \
  do {                                                                         \
    ASSERT_SYM_RESOLVE((_b_), (_name_));                                       \
    ASSERT_EQ(_s_->getValue().isDefined(), (_exp_));                           \
  } while (0)

/// \brief Asserts that a symbol is undefined.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_UNDEFINED(_b_, _name_)                                      \
  ASSERT_SYM_DEFINEDNESS((_b_), (_name_), false)

/// \brief Asserts that a symbol is defined.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_DEFINED(_b_, _name_)                                        \
  ASSERT_SYM_DEFINEDNESS((_b_), (_name_), true)

/// \brief Asserts the overwrittenness of a symbol.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_OVERWRITTEN(_b_, _name_, _exp_)                             \
  do {                                                                         \
    ASSERT_SYM_RESOLVE((_b_), (_name_));                                       \
    ASSERT_TRUE(_s_->getValue().getOverwriteName().has_value() == _exp_);      \
  } while (0)

/// \brief Asserts that a symbol name will be overwritten.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_WILL_BE_RENAMED(_b_, _name_)                                \
  ASSERT_SYM_OVERWRITTEN((_b_), (_name_), true)

/// \brief Asserts that a symbol name will NOT be overwritten.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_WILL_NOT_BE_RENAMED(_b_, _name_)                            \
  ASSERT_SYM_OVERWRITTEN((_b_), (_name_), false)

} // end anonymous namespace

using namespace saq::bartleby;

TEST(BartleByObjectYamlELF, Object386) {
  llvm::SmallVector<llvm::object::OwningBinary<llvm::object::Binary>, 2>
      Objects;
  ASSERT_TRUE(YAML2Objects("symbols_visibility.yaml",
                           llvm::Triple::ObjectFormatType::ELF, Objects, 2));

  llvm::DebugFlag = true;
  Bartleby B;
  auto Err = B.addBinary(std::move(Objects[0]));
  ASSERT_FALSE(Err);

  ASSERT_SYM_DEFINED(B, "defined_local_symbol");
  ASSERT_SYM_LOCAL(B, "defined_local_symbol");

  ASSERT_SYM_DEFINED(B, "defined_global_symbol");
  ASSERT_SYM_GLOBAL(B, "defined_global_symbol");

  ASSERT_SYM_UNDEFINED(B, "undefined_symbol");
  ASSERT_SYM_GLOBAL(B, "undefined_symbol");

  ASSERT_SYM_UNDEFINED(B, "weak_symbol");
  ASSERT_SYM_LOCAL(B, "weak_symbol");

  B.prefixGlobalAndDefinedSymbols("prefix_");

  ASSERT_SYM_WILL_NOT_BE_RENAMED(B, "defined_local_symbol");
  ASSERT_SYM_WILL_BE_RENAMED(B, "defined_global_symbol");
  ASSERT_SYM_WILL_NOT_BE_RENAMED(B, "undefined_symbol");
  ASSERT_SYM_WILL_NOT_BE_RENAMED(B, "weak_symbol");

  Err = B.addBinary(std::move(Objects[1]));
  ASSERT_FALSE(Err);

  ASSERT_SYM_DEFINED(B, "defined_local_symbol");
  ASSERT_SYM_LOCAL(B, "defined_local_symbol");

  ASSERT_SYM_DEFINED(B, "defined_global_symbol");
  ASSERT_SYM_GLOBAL(B, "defined_global_symbol");

  // Object 2 defined `undefined_symbol`.
  ASSERT_SYM_DEFINED(B, "undefined_symbol");
  ASSERT_SYM_GLOBAL(B, "undefined_symbol");

  ASSERT_SYM_UNDEFINED(B, "weak_symbol");
  ASSERT_SYM_LOCAL(B, "weak_symbol");

  B.prefixGlobalAndDefinedSymbols("prefix_");

  ASSERT_SYM_WILL_NOT_BE_RENAMED(B, "defined_local_symbol");
  ASSERT_SYM_WILL_BE_RENAMED(B, "defined_global_symbol");
  ASSERT_SYM_WILL_BE_RENAMED(B, "undefined_symbol");
  ASSERT_SYM_WILL_NOT_BE_RENAMED(B, "weak_symbol");

  auto ArOrErr = Bartleby::buildFinalArchive(std::move(B));
  ASSERT_TRUE(!!ArOrErr);
  auto ArContent = std::move(*ArOrErr);

  auto Ar = llvm::object::createBinary(*ArContent);
  ASSERT_TRUE(!!Ar);

  auto OwningAr = llvm::object::OwningBinary<llvm::object::Binary>(
      std::move(*Ar), std::move(ArContent));

  B = Bartleby();
  ASSERT_FALSE(B.addBinary(std::move(OwningAr)));

  ASSERT_SYM_DEFINED(B, "defined_local_symbol");
  ASSERT_SYM_LOCAL(B, "defined_local_symbol");

  ASSERT_SYM_DEFINED(B, "prefix_defined_global_symbol");
  ASSERT_SYM_GLOBAL(B, "prefix_defined_global_symbol");

  ASSERT_SYM_DEFINED(B, "prefix_undefined_symbol");
  ASSERT_SYM_GLOBAL(B, "prefix_undefined_symbol");

  ASSERT_SYM_UNDEFINED(B, "weak_symbol");
  ASSERT_SYM_LOCAL(B, "weak_symbol");
}

/// \brief Test that passing two objects with different format types is
/// an error.
TEST(BartleByObjectYamlError, ObjectTypeMisMatch) {
  llvm::SmallVector<llvm::object::OwningBinary<llvm::object::Binary>, 2>
      Objects;
  ASSERT_TRUE(YAML2Objects("arm64.yaml", llvm::Triple::ObjectFormatType::MachO,
                           Objects));

  ASSERT_TRUE(YAML2Objects("simple_x86_64.yaml",
                           llvm::Triple::ObjectFormatType::ELF, Objects));

  Bartleby B;
  ASSERT_FALSE(B.addBinary(std::move(Objects[0])));
  auto Err = B.addBinary(std::move(Objects[1]));
  ASSERT_TRUE(!!Err);
}
