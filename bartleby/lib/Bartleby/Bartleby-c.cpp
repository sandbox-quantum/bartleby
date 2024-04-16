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
/// \brief Bartleby C API implementation.
///
/// \author thb-sb

#include "Bartleby-c/Bartleby.h"
#include "Bartleby/Bartleby.h"

#include "llvm/Object/Binary.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"

#include <memory>

extern "C" {

#include <errno.h>
#include <stdlib.h>

} // end extern "C"

namespace bartleby = saq::bartleby;

/// \brief Definition of the Bartleby handle.
struct BartlebyHandle {
  /// Bartleby object.
  bartleby::Bartleby B;
};

extern "C" {

struct BartlebyHandle *saq_bartleby_new(void) { return new BartlebyHandle{}; }

void saq_bartleby_free(struct BartlebyHandle *bh) { delete bh; }

int saq_bartleby_set_prefix(struct BartlebyHandle *bh, const char *prefix) {
  if (bh == nullptr) {
    return EINVAL;
  }

  if (prefix == nullptr) {
    return EINVAL;
  }

  bh->B.prefixGlobalAndDefinedSymbols(prefix);

  return 0;
}

int saq_bartleby_add_binary(struct BartlebyHandle *bh, const void *s,
                            const size_t n) {
  if (bh == nullptr) {
    return EINVAL;
  }

  if (s == nullptr) {
    return EINVAL;
  }

  if (n == 0) {
    return EINVAL;
  }
  llvm::SmallVector<char, 2048> ObjContent(static_cast<const uint8_t *>(s),
                                           static_cast<const uint8_t *>(s) + n);
  auto OutBuffer = std::make_unique<llvm::SmallVectorMemoryBuffer>(
      std::move(ObjContent), false);

  if (auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(*OutBuffer)) {
    if (auto Err =
            bh->B.addBinary({std::move(*ObjOrErr), std::move(OutBuffer)})) {
      return EINVAL;
    }
  } else {
    return EINVAL;
  }

  return 0;
}

int saq_bartleby_build_archive(struct BartlebyHandle *bh, void **s, size_t *n) {
  std::unique_ptr<struct BartlebyHandle> handle(bh);

  if (handle == nullptr) {
    return EINVAL;
  }

  if (s == nullptr) {
    return EINVAL;
  }
  *s = nullptr;

  if (n == nullptr) {
    return EINVAL;
  }
  *n = 0;

  if (auto ArOrErr =
          bartleby::Bartleby::buildFinalArchive(std::move(handle->B))) {
    const auto MemBuf = std::move(*ArOrErr);
    *n = MemBuf->getBufferSize();
    *s = ::malloc(*n);
    if (*s == nullptr) {
      return ENOMEM;
    }
    ::memcpy(*s, MemBuf->getBufferStart(), *n);
    return 0;
  }
  return EINVAL;
}

} // end extern "C"
