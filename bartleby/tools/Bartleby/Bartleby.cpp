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
/// \brief Bartleby tool implementation.
///
/// \author thb-sb

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/WithColor.h"

#include "Bartleby/Bartleby.h"

/// \brief Input file.
llvm::cl::list<std::string> InputFileNames(llvm::cl::Required, "if",
                                           llvm::cl::desc("Input filename"),
                                           llvm::cl::value_desc("filename"));

/// \brief Prefix to apply.
llvm::cl::opt<std::string>
    Prefix("prefix",
           llvm::cl::desc("Prefix to set to global and defined symbols"),
           llvm::cl::value_desc("prefix"));

/// \brief Output file.
llvm::cl::opt<std::string> OutputFileName(llvm::cl::Required, "of",
                                          llvm::cl::desc("Output filename"),
                                          llvm::cl::value_desc("filename"));

/// \brief Display the list of symbols.
llvm::cl::opt<bool>
    DisplaySymbolList("display-symbols",
                      llvm::cl::desc("Display list of symbols"));

namespace bartleby = saq::bartleby;

namespace {

/// \brief Tool name;
constexpr llvm::StringRef ToolName = "bartleby";

[[noreturn]] void reportError(llvm::Twine message) noexcept {
  llvm::WithColor::error(llvm::errs(), ToolName) << message << '\n';
  llvm::errs().flush();
  std::exit(EXIT_FAILURE);
}

[[noreturn]] void reportError(llvm::Error e) noexcept {
  assert(e);
  std::string buf;
  llvm::raw_string_ostream os(buf);
  llvm::logAllUnhandledErrors(std::move(e), os);
  os.flush();
  reportError(buf);
}

[[noreturn]] void reportError(llvm::StringRef filepath, llvm::Error e) {
  assert(e);
  std::string buf;
  llvm::raw_string_ostream os(buf);
  llvm::logAllUnhandledErrors(std::move(e), os);
  os.flush();
  llvm::WithColor::error(llvm::errs(), ToolName)
      << "'" << filepath << "': " << buf;
  std::exit(EXIT_FAILURE);
}

/// \brief Collect all input files.
///
/// \return The bartleby handle, or nullopt if an error occurred.
[[nodiscard]] bartleby::Bartleby CollectObjects() noexcept {
  bartleby::Bartleby b;

  for (const auto &input_file : InputFileNames) {
    if (auto owned_binary = llvm::object::createBinary(input_file);
        !owned_binary) {
      reportError(input_file, owned_binary.takeError());
    } else if (auto err = b.AddBinary(std::move(owned_binary.get()))) {
      reportError(input_file, std::move(err));
    }
  }

  return b;
}

/// \brief Display symbols.
///
/// \param b Bartleby handle.
void DisplaySymbols(const bartleby::Bartleby &b) noexcept {
  const auto end = b.Symbols().end();
  for (auto entry = b.Symbols().begin(); entry != end; ++entry) {
    const auto &name = entry->first();
    const auto &sym = entry->getValue();
    const auto defined = sym.Defined();
    const auto global = sym.Global();
    llvm::outs() << "Symbol " << name << " is "
                 << (defined ? "defined" : "undefined") << " and "
                 << (global ? "global" : "local");

    if (defined && global && !Prefix.empty()) {
      llvm::outs() << " (to be prefixed by " << Prefix << ')';
    } else {
      llvm::outs() << " (left unchanged)";
    }
    llvm::outs() << '\n';
  }
}

} // end anonymous namespace

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  auto b = CollectObjects();

  if (!Prefix.empty()) {
    const auto n = b.PrefixGlobalAndDefinedSymbols(Prefix);
    llvm::outs() << n << " symbol(s) prefixed\n";
  }

  if (DisplaySymbolList) {
    DisplaySymbols(b);
  }

  if (auto err =
          bartleby::Bartleby::BuildFinalArchive(std::move(b), OutputFileName);
      err) {
    reportError(std::move(err));
  }
  llvm::outs() << OutputFileName << " produced.\n";

  return EXIT_SUCCESS;
}
