mod prop_tests;
mod simple_tests;

use core::ffi::c_void;
use fixed_malloc::ffi::*;
use std::alloc::{alloc_zeroed, dealloc, Layout};

pub fn init(memory_size: usize) -> (*mut c_void, Layout) {
    let layout = Layout::from_size_align(memory_size, FM_PAGE_SIZE).expect("layout");
    let buffer = unsafe { alloc_zeroed(layout) };
    let ret = unsafe { fm_sm_reinit(buffer as *mut c_void, memory_size, 1) };
    assert_eq!(ret, 0);
    (buffer as *mut c_void, layout)
}

pub fn deinit(meta: (*mut c_void, Layout)) {
    unsafe { dealloc(meta.0 as *mut u8, meta.1) };
}

pub fn assert_valid_pointers(pointers: &[(*mut c_void, usize)]) {
    let mut pointers: Vec<(usize, usize)> =
        pointers.iter().map(|(a, s)| (*a as usize, *s)).collect();

    let buffer_start = unsafe { fm_lm_test_buffer_pointer() } as usize;
    let buffer_size = unsafe { fm_lm_test_total_buffer_size() };
    let buffer_end = buffer_start + buffer_size;

    for (a, s) in pointers.clone() {
        assert!(
            a % 16 == 0,
            "Pointer {:x} is not aligned on 16-byte boundary!",
            a
        );
        assert!(
            a >= buffer_start && a + s <= buffer_end,
            "Pointer {:x} exceeds buffer range {:x} - {:x}!",
            a,
            buffer_start,
            buffer_end,
        );
    }

    pointers.sort_by_key(|(a, _)| *a);

    for i in 0..pointers.len() - 1 {
        assert!(
            pointers[i].0 + pointers[i].1 <= pointers[i + 1].0,
            "Pointer {:x} and {:x} collides!",
            pointers[i].0,
            pointers[i + 1].0
        );
    }
}
