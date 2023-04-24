use super::*;
use fixed_malloc::ffi::*;
use rusty_fork::rusty_fork_test;

rusty_fork_test! {

#[test]
fn test_reinit() {
    assert_eq!(unsafe { fm_lm_test_total_buffer_size() }, 655360);

    let m = init(32 * 4096);
    assert_eq!(unsafe { fm_lm_test_total_buffer_size() }, 32 * 4096);
    assert_eq!(unsafe { fm_lm_test_buffer_pointer() }, m.0);
    deinit(m);
}

#[test]
fn test_simple_malloc_free() {
    let p1 = unsafe { fm_sm_malloc(17) };
    assert!(!p1.is_null());
    let p2 = unsafe { fm_sm_malloc(32) };
    assert!(!p2.is_null());
    let p3 = unsafe { fm_sm_malloc(5000) };
    assert!(!p3.is_null());

    unsafe { fm_sm_free(p2); }
    unsafe { fm_sm_free(p3); }
    unsafe { fm_sm_free(p1); }
}

#[test]
fn test_repeated_malloc() {
    for _ in 0..5000 {
        let p = unsafe { fm_sm_malloc(32) };
        assert!(!p.is_null());
    }
}

#[test]
fn test_malloc_biggest() {
    let p = unsafe { fm_sm_malloc(651264) };
    assert!(!p.is_null());
}

#[test]
fn test_malloc_free_then_malloc_biggest() {
    let p1 = unsafe { fm_sm_malloc(17) };
    assert!(!p1.is_null());
    let p2 = unsafe { fm_sm_malloc(32) };
    assert!(!p2.is_null());
    let p3 = unsafe { fm_sm_malloc(5000) };
    assert!(!p3.is_null());
    let p4 = unsafe { fm_sm_malloc(651264) };
    assert!(p4.is_null());

    unsafe { fm_sm_free(p1); }
    unsafe { fm_sm_free(p2); }
    unsafe { fm_sm_free(p3); }

    let p4 = unsafe { fm_sm_malloc(651264) };
    assert!(!p4.is_null());
}

}
