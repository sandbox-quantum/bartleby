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

#include <sstream>

#include "Bartleby/Error.h"

namespace saq::bartleby {

char Error::ID = 'b';

void Error::log(llvm::raw_ostream &os) const noexcept {
  os << message();
  std::visit(
      [&os](auto &&err) {
        using ErrT = std::decay_t<decltype(err)>;
        if constexpr (std::is_same_v<UnsupportedBinaryReason, ErrT>) {
          os << err.msg;
        } else if constexpr (std::is_same_v<ObjectFormatTypeMismatchReason,
                                            ErrT>) {
          os << "expected " << err.constraint << ", got " << err.type;
        } else {
          __builtin_unreachable();
        }
      },
      _reason);
}

std::string Error::message() const noexcept {
  std::stringstream ss;
  std::visit(
      [&ss](auto &&err) {
        using ErrT = std::decay_t<decltype(err)>;
        if constexpr (std::is_same_v<UnsupportedBinaryReason, ErrT>) {
          ss << "error while reading binary: " << err.msg.str().data();
        } else if constexpr (std::is_same_v<ObjectFormatTypeMismatchReason,
                                            ErrT>) {
          ss << "invalid object format type: expected " << err.constraint
             << ", got " << err.type;
        } else {
          __builtin_unreachable();
        }
      },
      _reason);
  return ss.str();
}

std::error_code Error::convertToErrorCode() const noexcept {
  return std::visit(
      [](auto &&err) {
        using ErrT = std::decay_t<decltype(err)>;
        if constexpr (std::is_same_v<UnsupportedBinaryReason, ErrT>) {
          return std::error_code(1, std::system_category());
        } else if constexpr (std::is_same_v<ObjectFormatTypeMismatchReason,
                                            ErrT>) {
          return std::error_code(2, std::system_category());
        } else {
          __builtin_unreachable();
        }
      },
      _reason);
}

} // end namespace saq::bartleby
