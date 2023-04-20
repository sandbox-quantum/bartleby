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
/// \brief Error implementation.
///
/// \author thb-sb

#include "Bartleby/Error.h"

using namespace saq::bartleby;

char Error::ID = 'b';

void Error::log(llvm::raw_ostream &OS) const noexcept {
  OS << message();
  std::visit(
      [&OS](auto &&Err) {
        using ErrT = std::decay_t<decltype(Err)>;
        if constexpr (std::is_same_v<UnsupportedBinaryReason, ErrT>) {
          OS << Err.Msg;
        } else if constexpr (std::is_same_v<ObjectFormatTypeMismatchReason,
                                            ErrT>) {
          OS << "expected " << Err.Constraint << ", got " << Err.Found;
        } else if constexpr (std::is_same_v<MachOUniversalBinaryReason, ErrT>) {
          OS << Err.Msg;
        } else {
          __builtin_unreachable();
        }
      },
      Reason);
}

std::string Error::message() const noexcept {
  std::string Msg;
  llvm::raw_string_ostream OS(Msg);
  std::visit(
      [&OS](auto &&Err) {
        using ErrT = std::decay_t<decltype(Err)>;
        if constexpr (std::is_same_v<UnsupportedBinaryReason, ErrT>) {
          OS << "error while reading binary: " << Err.Msg;
        } else if constexpr (std::is_same_v<ObjectFormatTypeMismatchReason,
                                            ErrT>) {
          OS << "invalid object format type: expected " << Err.Constraint
             << ", got " << Err.Found;
        } else if constexpr (std::is_same_v<MachOUniversalBinaryReason, ErrT>) {
          OS << "fat Mach-O error: " << Err.Msg;
        } else {
          __builtin_unreachable();
        }
      },
      Reason);
  return Msg;
}

std::error_code Error::convertToErrorCode() const noexcept {
  return std::visit(
      [](auto &&Err) {
        using ErrT = std::decay_t<decltype(Err)>;
        if constexpr (std::is_same_v<UnsupportedBinaryReason, ErrT>) {
          return std::error_code(1, std::system_category());
        } else if constexpr (std::is_same_v<ObjectFormatTypeMismatchReason,
                                            ErrT>) {
          return std::error_code(2, std::system_category());
        } else if constexpr (std::is_same_v<MachOUniversalBinaryReason, ErrT>) {
          return std::error_code(3, std::system_category());
        } else {
          __builtin_unreachable();
        }
      },
      Reason);
}
