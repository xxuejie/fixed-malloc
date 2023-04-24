#![no_std]

pub mod ffi;

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;

pub fn reinitialize(buffer: *mut u8, len: usize, zero_filled: bool) {
    let ret = unsafe {
        crate::ffi::fm_sm_reinit(buffer as *mut c_void, len, if zero_filled { 1 } else { 0 })
    };
    assert_eq!(ret, 0, "Initialization failure: {}", ret);
}

pub struct FixedAlloc {}

impl FixedAlloc {
    // Initialize using static memory
    #[cfg(not(feature = "manual-init"))]
    pub fn new_static() -> Self {
        Self {}
    }

    pub fn new(buffer: *mut u8, len: usize, zero_filled: bool) -> Self {
        reinitialize(buffer, len, zero_filled);
        Self {}
    }
}

unsafe impl GlobalAlloc for FixedAlloc {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        ffi::fm_sm_malloc(layout.size()) as *mut u8
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        ffi::fm_sm_free(ptr as *mut c_void)
    }

    unsafe fn realloc(&self, ptr: *mut u8, _layout: Layout, new_size: usize) -> *mut u8 {
        ffi::fm_sm_realloc(ptr as *mut c_void, new_size) as *mut u8
    }
}
