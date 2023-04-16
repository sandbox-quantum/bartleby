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

#include <unistd.h>

#include "gtest/gtest.h"

#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Object/Binary.h"
#include "llvm/ObjectYAML/yaml2obj.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/YAMLParser.h"

#include "Bartleby/Bartleby.h"

namespace {

/// \brief Base directory for test data based on yaml files.
#define TEST_DATA_BASE_DIR "bartleby/tests/Bartleby/testdata/yaml"

/// \brief Size for objects. Used by llvm::SmallVector.
constexpr size_t kObjectSize = 2048;

/// \brief Convert a llvm::Triple::ObjectFormatType into a path to a
/// sub-directory in kTestDataBaseDir.
///
/// \param format Object format.
///
/// \return Path to the directory containing yaml files for the format.
[[nodiscard]] llvm::StringRef
ObjectFormatToPath(const llvm::Triple::ObjectFormatType format) {
  switch (format) {
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

/// \brief Read a file under the directory of a specific format object.
///
/// \param filepath Filepath
/// \param object_format Object format.
/// \param[out] content Content of the file.
///
/// \return An assertion result.
[[nodiscard]] testing::AssertionResult
ReadYamlFile(llvm::StringRef filepath,
             const llvm::Triple::ObjectFormatType object_format,
             std::unique_ptr<llvm::MemoryBuffer> &content) {
  if (auto file_or_err = llvm::MemoryBuffer::getFile(
          ObjectFormatToPath(object_format) + "/" + filepath, true)) {
    content = std::move(file_or_err.get());
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << "failed to read " << filepath.data()
                                       << ": " << file_or_err.getError();
  }
}

/// \brief Parse a yaml file and fill a vector with some llvm::ObjectFile.
///
/// \param path Filepath to the yaml file.
/// \param object_format Object format.
/// \param[out] objects Vector of objects to fill.
///
/// \return An assertion.
[[nodiscard]] testing::AssertionResult YAML2Objects(
    llvm::StringRef filepath, llvm::Triple::ObjectFormatType object_format,
    llvm::SmallVectorImpl<llvm::object::OwningBinary<llvm::object::Binary>>
        &objects) {
  std::unique_ptr<llvm::MemoryBuffer> content;
  if (auto err = ReadYamlFile(filepath, object_format, content); !err) {
    return testing::AssertionFailure()
           << "failed to read file: " << err.message();
  }

  llvm::SourceMgr mgr;
  std::error_code ec;
  llvm::yaml::Stream s(content->getBuffer(), mgr, true, &ec);
  if (ec) {
    return testing::AssertionFailure()
           << "failed to parse yaml stream: " << ec.message();
  }

  for (auto &doc : s) {
    const llvm::yaml::Node *root;
    if (root = doc.getRoot(); !root) {
      return testing::AssertionFailure()
             << "failed to parse node: " << ec.message();
    }

    std::unique_ptr<llvm::object::ObjectFile> obj;
    llvm::SmallVector<char, kObjectSize> content;
    if (auto obj_or_err = llvm::yaml::yaml2ObjectFile(
            content, root->getRawTag().data(), nullptr)) {
      obj = std::move(obj_or_err);
    } else {
      return testing::AssertionFailure() << "failed to convert yaml to Object";
    }
    if (obj->getTripleObjectFormat() != object_format) {
      return testing::AssertionFailure()
             << "yaml object type " << obj->getTripleObjectFormat()
             << " doesn't match " << object_format;
    }
    objects.emplace_back(std::move(obj),
                         std::make_unique<llvm::SmallVectorMemoryBuffer>(
                             std::move(content), false));
  }
  return testing::AssertionSuccess();
}

/// \brief Resolve symbol in map.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
///
/// \return _s_ Symbol entry.
#define ASSERT_SYM_RESOLVE(_b_, _name_)                                        \
  const auto &_s_ = (_b_).Symbols().find((_name_));                            \
  ASSERT_NE(_s_, (_b_).Symbols().end()) << "Symbol " << (_name_) << " not found"

/// \brief Assert symbol globalness.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_GLOBALNESS(_b_, _name_, _exp_)                              \
  do {                                                                         \
    ASSERT_SYM_RESOLVE((_b_), (_name_));                                       \
    ASSERT_EQ(_s_->getValue().Global(), (_exp_));                              \
  } while (0)

/// \brief Assert that a symbol is local.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_LOCAL(_b_, _name_)                                          \
  ASSERT_SYM_GLOBALNESS((_b_), (_name_), false)

/// \brief Assert that a symbol is local.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_GLOBAL(_b_, _name_)                                         \
  ASSERT_SYM_GLOBALNESS((_b_), (_name_), true)

/// \brief Assert symbol definedness.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_DEFINEDNESS(_b_, _name_, _exp_)                             \
  do {                                                                         \
    ASSERT_SYM_RESOLVE((_b_), (_name_));                                       \
    ASSERT_EQ(_s_->getValue().Defined(), (_exp_));                             \
  } while (0)

/// \brief Assert that a symbol is undefined.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_UNDEFINED(_b_, _name_)                                      \
  ASSERT_SYM_DEFINEDNESS((_b_), (_name_), false)

/// \brief Assert that a symbol is defined.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_DEFINED(_b_, _name_)                                        \
  ASSERT_SYM_DEFINEDNESS((_b_), (_name_), true)

/// \brief Assert symbol overwrittenness.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_OVERWRITTEN(_b_, _name_, _exp_)                             \
  do {                                                                         \
    ASSERT_SYM_RESOLVE((_b_), (_name_));                                       \
    ASSERT_TRUE(_s_->getValue().OverwriteName().has_value() == _exp_);         \
  } while (0)

/// \brief Assert that a symbol name will be overwritten.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_WILL_BE_RENAMED(_b_, _name_)                                \
  ASSERT_SYM_OVERWRITTEN((_b_), (_name_), true)

/// \brief Assert that a symbol name will NOT be overwritten.
///
/// \param _b_ Bartleby handle.
/// \param _name_ Name of the symbol.
#define ASSERT_SYM_WILL_NOT_BE_RENAMED(_b_, _name_)                            \
  ASSERT_SYM_OVERWRITTEN((_b_), (_name_), false)

} // end anonymous namespace

using namespace saq::bartleby;

TEST(BartleByObjectYamlELF, Object386) {
  llvm::SmallVector<llvm::object::OwningBinary<llvm::object::Binary>, 2>
      objects;
  ASSERT_TRUE(YAML2Objects("symbols_visibility.yaml",
                           llvm::Triple::ObjectFormatType::ELF, objects));

  llvm::DebugFlag = true;
  Bartleby b;
  auto err = b.AddBinary(std::move(objects[0]));
  ASSERT_FALSE(err);

  ASSERT_SYM_DEFINED(b, "defined_local_symbol");
  ASSERT_SYM_LOCAL(b, "defined_local_symbol");

  ASSERT_SYM_DEFINED(b, "defined_global_symbol");
  ASSERT_SYM_GLOBAL(b, "defined_global_symbol");

  ASSERT_SYM_UNDEFINED(b, "undefined_symbol");
  ASSERT_SYM_GLOBAL(b, "undefined_symbol");

  ASSERT_SYM_UNDEFINED(b, "weak_symbol");
  ASSERT_SYM_LOCAL(b, "weak_symbol");

  b.PrefixGlobalAndDefinedSymbols("prefix_");

  ASSERT_SYM_WILL_NOT_BE_RENAMED(b, "defined_local_symbol");
  ASSERT_SYM_WILL_BE_RENAMED(b, "defined_global_symbol");
  ASSERT_SYM_WILL_NOT_BE_RENAMED(b, "undefined_symbol");
  ASSERT_SYM_WILL_NOT_BE_RENAMED(b, "weak_symbol");

  err = b.AddBinary(std::move(objects[1]));
  ASSERT_FALSE(err);

  ASSERT_SYM_DEFINED(b, "defined_local_symbol");
  ASSERT_SYM_LOCAL(b, "defined_local_symbol");

  ASSERT_SYM_DEFINED(b, "defined_global_symbol");
  ASSERT_SYM_GLOBAL(b, "defined_global_symbol");

  // Object 2 defined `undefined_symbol`.
  ASSERT_SYM_DEFINED(b, "undefined_symbol");
  ASSERT_SYM_GLOBAL(b, "undefined_symbol");

  ASSERT_SYM_UNDEFINED(b, "weak_symbol");
  ASSERT_SYM_LOCAL(b, "weak_symbol");

  b.PrefixGlobalAndDefinedSymbols("prefix_");

  ASSERT_SYM_WILL_NOT_BE_RENAMED(b, "defined_local_symbol");
  ASSERT_SYM_WILL_BE_RENAMED(b, "defined_global_symbol");
  ASSERT_SYM_WILL_BE_RENAMED(b, "undefined_symbol");
  ASSERT_SYM_WILL_NOT_BE_RENAMED(b, "weak_symbol");

  auto ar_or_err = Bartleby::BuildFinalArchive(std::move(b));
  ASSERT_TRUE(!!ar_or_err);
  auto ar_content = std::move(ar_or_err.get());

  auto ar = llvm::object::createBinary(*ar_content);
  ASSERT_TRUE(!!ar);

  auto owning_ar = llvm::object::OwningBinary<llvm::object::Binary>(
      std::move(ar.get()), std::move(ar_content));

  b = Bartleby();
  ASSERT_FALSE(b.AddBinary(std::move(owning_ar)));

  ASSERT_SYM_DEFINED(b, "defined_local_symbol");
  ASSERT_SYM_LOCAL(b, "defined_local_symbol");

  ASSERT_SYM_DEFINED(b, "prefix_defined_global_symbol");
  ASSERT_SYM_GLOBAL(b, "prefix_defined_global_symbol");

  ASSERT_SYM_DEFINED(b, "prefix_undefined_symbol");
  ASSERT_SYM_GLOBAL(b, "prefix_undefined_symbol");

  ASSERT_SYM_UNDEFINED(b, "weak_symbol");
  ASSERT_SYM_LOCAL(b, "weak_symbol");
}

/// \brief Test that passing two objects with different format types is
/// an error.
TEST(BartleByObjectYamlError, ObjectTypeMisMatch) {
  llvm::SmallVector<llvm::object::OwningBinary<llvm::object::Binary>, 2>
      objects;
  ASSERT_TRUE(YAML2Objects("arm64.yaml", llvm::Triple::ObjectFormatType::MachO,
                           objects));

  ASSERT_TRUE(YAML2Objects("simple_x86_64.yaml",
                           llvm::Triple::ObjectFormatType::ELF, objects));

  Bartleby b;
  ASSERT_FALSE(b.AddBinary(std::move(objects[0])));
  auto err = b.AddBinary(std::move(objects[1]));
  ASSERT_TRUE(!!err);
}
