use core::ffi::{c_int, c_void};

pub const FM_PAGE_SHIFT: usize = 12;
pub const FM_PAGE_SIZE: usize = 1 << FM_PAGE_SHIFT;

pub const FM_LM_T_TRANSIENT: c_int = 0x1;
pub const FM_LM_T_PERSISTENT: c_int = 0x2;

#[link(name = "fixed-malloc", kind = "static")]
extern "C" {
    pub fn fm_sm_reinit(buffer: *mut c_void, size: usize, zero_filled: c_int) -> c_int;
    pub fn fm_sm_malloc(size: usize) -> *mut c_void;
    pub fn fm_sm_free(ptr: *mut c_void);
    pub fn fm_sm_realloc(ptr: *mut c_void, size: usize) -> *mut c_void;

    pub fn fm_lm_reinit(buffer: *mut c_void, size: usize, zero_filled: c_int) -> c_int;
    pub fn fm_lm_malloc(size: usize, t: c_int) -> *mut c_void;
    pub fn fm_lm_free(ptr: *mut c_void);
    pub fn fm_lm_realloc(ptr: *mut c_void, size: usize, t: c_int) -> *mut c_void;
}

#[cfg(feature = "test-support")]
#[link(name = "fixed-malloc", kind = "static")]
extern "C" {
    pub fn fm_lm_test_buffer_pointer() -> *mut c_void;
    pub fn fm_lm_test_total_buffer_size() -> usize;
}
