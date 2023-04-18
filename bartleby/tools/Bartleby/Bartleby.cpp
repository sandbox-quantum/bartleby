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

#include "Bartleby/Bartleby.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/WithColor.h"

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

/// \brief Displays the list of symbols.
llvm::cl::opt<bool>
    DisplaySymbolList("display-symbols",
                      llvm::cl::desc("Display list of symbols"));

namespace bartleby = saq::bartleby;

namespace {

/// \brief Tool name;
constexpr llvm::StringRef ToolName = "bartleby";

/// \brief Reports an error by displaying a message.
///
/// \param Message Message to display as an error string.
[[noreturn]] void reportError(llvm::Twine Message) noexcept {
  llvm::WithColor::error(llvm::errs(), ToolName) << Message << '\n';
  llvm::errs().flush();
  std::exit(EXIT_FAILURE);
}

/// \brief Reports an error by extracting the message from a \p llvm::Error.
///
/// \param E The error to report.
[[noreturn]] void reportError(llvm::Error E) noexcept {
  assert(E);
  std::string Buf;
  llvm::raw_string_ostream OS(Buf);
  llvm::logAllUnhandledErrors(std::move(E), OS);
  OS.flush();
  reportError(Buf);
}

/// \brief Reports an error related to a certain file manipulation.
///
/// \param Filepath The file involved in the error.
/// \param E The error to report.
[[noreturn]] void reportError(llvm::StringRef Filepath, llvm::Error E) {
  assert(E);
  std::string Buf;
  llvm::raw_string_ostream OS(Buf);
  llvm::logAllUnhandledErrors(std::move(E), OS);
  OS.flush();
  llvm::WithColor::error(llvm::errs(), ToolName)
      << "'" << Filepath << "': " << Buf;
  std::exit(EXIT_FAILURE);
}

/// \brief Collects all input files.
///
/// \returns The bartleby handle, or nullopt if an error occurred.
[[nodiscard]] bartleby::Bartleby CollectObjects() noexcept {
  bartleby::Bartleby B;

  for (const auto &InputFile : InputFileNames) {
    if (auto OwnedBinary = llvm::object::createBinary(InputFile);
        !OwnedBinary) {
      reportError(InputFile, OwnedBinary.takeError());
    } else if (auto Err = B.addBinary(std::move(*OwnedBinary))) {
      reportError(InputFile, std::move(Err));
    }
  }

  return B;
}

/// \brief Displays the symbols previously collected.
///
/// \param B Bartleby handle.
void displaySymbols(const bartleby::Bartleby &B) noexcept {
  const auto End = B.getSymbols().end();
  for (auto Entry = B.getSymbols().begin(); Entry != End; ++Entry) {
    const auto &Name = Entry->first();
    const auto &Sym = Entry->getValue();
    const auto Defined = Sym.isDefined();
    const auto Global = Sym.isGlobal();
    llvm::outs() << "Symbol " << Name << " is "
                 << (Defined ? "defined" : "undefined") << " and "
                 << (Global ? "global" : "local");

    if (Defined && Global && !Prefix.empty()) {
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

  auto B = CollectObjects();

  if (!Prefix.empty()) {
    const auto N = B.prefixGlobalAndDefinedSymbols(Prefix);
    llvm::outs() << N << " symbol(s) prefixed\n";
  }

  if (DisplaySymbolList) {
    displaySymbols(B);
  }

  if (auto Err =
          bartleby::Bartleby::buildFinalArchive(std::move(B), OutputFileName)) {
    reportError(std::move(Err));
  }
  llvm::outs() << OutputFileName << " produced.\n";

  return EXIT_SUCCESS;
}
