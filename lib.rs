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

//! Bartleby Rust library, based on the C API.

/// Mutable pointer to a Bartleby handle.
type BartlebyHandleMutPtr = *mut std::ffi::c_void;

extern "C" {
    fn saq_bartleby_new() -> BartlebyHandleMutPtr;
    fn saq_bartleby_free(bh: BartlebyHandleMutPtr);
    fn saq_bartleby_set_prefix(bh: BartlebyHandleMutPtr, prefix: *const i8) -> std::ffi::c_int;
    fn saq_bartleby_add_binary(
        bh: BartlebyHandleMutPtr,
        s: *const std::ffi::c_void,
        n: usize,
    ) -> std::ffi::c_int;
    fn saq_bartleby_build_archive(
        bh: BartlebyHandleMutPtr,
        s: *mut *mut std::ffi::c_void,
        n: *mut usize,
    ) -> std::ffi::c_int;
}

/// A Bartleby handle.
#[derive(Debug)]
pub struct Bartleby(BartlebyHandleMutPtr);

/// Implements [`std::fmt::Display`] for [`Bartleby`].
impl std::fmt::Display for Bartleby {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Bartleby(c_ptr={:p})", self.0)
    }
}

/// Implements [`std::ops::Drop`] for [`Bartleby`].
impl std::ops::Drop for Bartleby {
    fn drop(&mut self) {
        unsafe {
            saq_bartleby_free(self.0);
        }
        self.0 = std::ptr::null_mut();
    }
}

/// Implements [`Bartleby`].
impl Bartleby {
    /// Constructs a new Bartleby.
    pub fn try_new() -> Result<Self, String> {
        let bh = unsafe { saq_bartleby_new() };
        if bh.is_null() {
            Err("`saq_bartleby_new` returned a null pointer.".into())
        } else {
            Ok(Self(bh))
        }
    }

    /// Applies a prefix to the global and defined symbols.
    pub fn set_prefix(&mut self, prefix: impl std::convert::AsRef<str>) -> Result<(), String> {
        let cstring = std::ffi::CString::new(prefix.as_ref())
            .map_err(|e| format!("`CString::new` failed: {e}"))?;

        match unsafe { saq_bartleby_set_prefix(self.0, cstring.as_ptr()) } {
            0 => Ok(()),
            n => Err(format!("`saq_bartleby_set_prefix` returned {n}")),
        }
    }

    /// Adds a binary to Bartleby.
    pub fn add_binary(&mut self, bin: impl std::convert::AsRef<[u8]>) -> Result<(), String> {
        let buf = bin.as_ref();
        match unsafe { saq_bartleby_add_binary(self.0, buf.as_ptr().cast(), buf.len()) } {
            0 => Ok(()),
            n => Err(format!("`saq_bartleby_add_binary` returned {n}")),
        }
    }

    /// Builds the final archive.
    pub fn into_archive(mut self) -> Result<Vec<u8>, String> {
        let mut out: *mut std::ffi::c_void = std::ptr::null_mut();
        let mut out_size: usize = 0;

        let r = unsafe {
            saq_bartleby_build_archive(self.0, &mut out as *mut _, &mut out_size as *mut _)
        };
        self.0 = std::ptr::null_mut();
        match r {
            0 => {
                let view: &[u8] = unsafe { std::slice::from_raw_parts(out.cast(), out_size) };
                Ok(Vec::from(view))
            }
            n => Err(format!("`saq_bartleby_build_archive` returned {n}")),
        }
    }
}
