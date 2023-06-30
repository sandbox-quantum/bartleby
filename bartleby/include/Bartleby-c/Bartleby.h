/** Copyright 2023 SandboxAQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http: *www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#ifndef SAQ_BARTLEBY_H
#define SAQ_BARTLEBY_H

#include <sys/types.h>

#if (defined(__clang__) || (_GNUC__ >= 4))
#define SAQ_BARTLEBY_API __attribute__((visibility("default")))
#else
#define SAQ_BARTLEBY_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 * \brief Bartleby C API specification.
 *
 * \author thb-sb */

/** \brief A Bartleby handle. */
struct BartlebyHandle;

/** \brief Allocates a new Bartleby handle.
 *
 * \returns A new Bartleby handle, or NULL if an error occurred. */
SAQ_BARTLEBY_API struct BartlebyHandle *saq_bartleby_new(void);

/** \brief Frees a Bartleby handle.
 *
 * \param bh Bartleby handle to free. A NULL value here is allowed. */
SAQ_BARTLEBY_API void saq_bartleby_free(struct BartlebyHandle *bh);

/** \brief Applies a prefix to all global and defined symbols.
 *
 * \param bh Bartleby handle.
 * \param prefix Prefix to apply.
 *
 * \returns 0 on success, else an error code. */
SAQ_BARTLEBY_API int saq_bartleby_set_prefix(struct BartlebyHandle *bh,
                                             const char *prefix);

/** \brief Adds a new binary to Bartleby.
 *
 * \param bh Bartleby handle.
 * \param s Buffer containing the binary.
 * \param n Size of `bin_src`.
 *
 * Buffer `s` is going to be copied, thus it can be freed after.
 *
 * \returns 0 on success, else an error code. */
SAQ_BARTLEBY_API int saq_bartleby_add_binary(struct BartlebyHandle *bh,
                                             const void *s, const size_t n);

/** \brief Builds the final archive and writes its content to a buffer.
 *
 * \warning This function consumes the input Bartleby handle. Thus, users
 *          must not call `saq_bartleby_free` after calling `saq_bartleby_build_archive`.
 *
 * \param bh Bartleby handle.
 * \param[out] s Destination buffer.
 * \param[out] n Size of `s`.
 *
 * \return 0 on success, else an error code. */
SAQ_BARTLEBY_API int saq_bartleby_build_archive(struct BartlebyHandle *bh,
                                                void **s, size_t *n);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SAQ_BARTLEBY_H
