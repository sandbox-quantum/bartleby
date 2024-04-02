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
/// \brief Error specification.
///
/// \author thb-sb

#pragma once

#include "Bartleby/Bartleby.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Error.h"
#include "llvm/TargetParser/Triple.h"

#include <variant>

namespace saq::bartleby {

/// \brief Custom error info for Bartleby.
class Error : public llvm::ErrorInfo<Error> {
public:
  /// \brief Error ID.
  static char ID;

  /// \brief Error from std::error_code.
  struct UnsupportedBinaryReason {
    /// \brief Error message.
    llvm::SmallString<32> Msg;
  };

  /// \brief Mismatch between object format type.
  ///
  /// This error is raised when an object has a different format type than
  /// the objects previously added to a Bartleby handle.
  struct ObjectFormatTypeMismatchReason {
    /// \brief Object type from the Bartleby handle.
    ObjectFormat Constraint;

    /// \brief The type of the object responsible of this error.
    ObjectFormat Found;
  };

  /// \brief Fat Mach-O related error.
  struct MachOUniversalBinaryReason {
    /// \brief Error message.
    llvm::SmallString<32> Msg;
  };

  /// \brief Reason for error.
  using ReasonT =
      std::variant<UnsupportedBinaryReason, ObjectFormatTypeMismatchReason,
                   MachOUniversalBinaryReason>;

  /// \brief Constructs an error using a reason.
  ///
  /// \param Reason The reason of the error.
  template <typename T> Error(T Reason) noexcept : Reason(std::move(Reason)) {}

  ~Error() noexcept override = default;

  void log(llvm::raw_ostream &OS) const noexcept override;
  std::string message() const noexcept override;
  std::error_code convertToErrorCode() const noexcept override;

private:
  /// \brief Reason.
  ReasonT Reason;
};

} // end namespace saq::bartleby
