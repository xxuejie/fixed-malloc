use super::*;
use fixed_malloc::ffi::*;
use proptest::prelude::*;
use rand::prelude::*;

fn gen_size(rng: &mut StdRng) -> usize {
    // We want 67% of alloced data to be smaller ones.
    if rng.gen_ratio(2, 3) {
        rng.gen_range(1..=1024)
    } else {
        rng.gen_range(1..=200000)
    }
}

proptest! {
    #![proptest_config(ProptestConfig {
        fork: true,
        .. ProptestConfig::default()
    })]

    #[test]
    fn test_simple_malloc(s in 0usize..651264usize) {
        let m = init(655360);

        let p = unsafe { fm_sm_malloc(s) };
        assert!(!p.is_null());

        deinit(m);
    }

    #[test]
    fn test_oversized_malloc(s in 651265usize..) {
        let m = init(655360);

        let p = unsafe { fm_sm_malloc(s) };
        assert!(p.is_null());

        deinit(m);
    }

    #[test]
    // ((655360 / 4096) - 1) * ((4096 - 64) / 32) == 20034
    fn test_multiple_simple_malloc(times in 1..=20034) {
        let m = init(655360);
        let mut ptrs = vec![];

        for _ in 0..times {
            let p = unsafe { fm_sm_malloc(32) };
            assert!(!p.is_null());
            ptrs.push((p, 32));
        }
        assert_valid_pointers(&ptrs);

        deinit(m);
    }

    #[test]
    fn test_multiple_different_sized_malloc(
        small_allocs in prop::collection::vec(1usize..=1024usize, 1..=200),
        big_allocs in prop::collection::vec(4096usize..=40960usize, 5..=10)) {
        let m = init(655360);
        let mut ptrs = vec![];

        for s in small_allocs {
            let p = unsafe { fm_sm_malloc(s) };
            assert!(!p.is_null());
            ptrs.push((p, s));
        }
        for b in big_allocs {
            let p = unsafe { fm_sm_malloc(b) };
            assert!(!p.is_null());
            ptrs.push((p, b));
        }
        assert_valid_pointers(&ptrs);

        deinit(m);
    }

    #[test]
    fn test_continous_malloc(seed in 0..=u64::MAX) {
        let m = init(655360);

        let mut rng = StdRng::seed_from_u64(seed);
        let times = rng.gen_range(10..20);

        let mut i = 0;
        let mut ptrs = vec![];
        let mut last_size = gen_size(&mut rng);
        while i < times {
            let p = unsafe { fm_sm_malloc(last_size) };
            assert!(!p.is_null());
            ptrs.push((p, last_size));

            loop {
                last_size = gen_size(&mut rng);
                let p = unsafe { fm_sm_malloc(last_size) };
                if p.is_null() {
                    break;
                }
                ptrs.push((p, last_size));
            }
            assert_valid_pointers(&ptrs);

            for p in ptrs.drain(..) {
                unsafe { fm_sm_free(p.0); }
            }

            // See if all memories are freed by allocating the biggest
            // page. Or we can leverage fixed-malloc's test support later.
            let p = unsafe { fm_sm_malloc(651264) };
            assert!(!p.is_null());
            unsafe { fm_sm_free(p); }
            i += 1;
        }

        deinit(m);
    }

    #[test]
    fn test_realloc(
        seed in 0..=u64::MAX,
        initial_allocs in prop::collection::vec(1usize..=200000usize, 15..=20),
        times in 20..100,
    ) {
        let m = init(12042240);

        let mut rng = StdRng::seed_from_u64(seed);
        let mut ptrs = vec![];
        for s in initial_allocs {
            let p = unsafe { fm_sm_malloc(s) };
            assert!(!p.is_null());
            ptrs.push((p, s));
        }
        assert_valid_pointers(&ptrs);

        for _ in 0..times {
            ptrs.shuffle(&mut rng);

            for i in 0..rng.gen_range(1..ptrs.len()) {
                let new_size = gen_size(&mut rng);
                let np = unsafe { fm_sm_realloc(ptrs[i].0, new_size) };
                assert!(!np.is_null());
                ptrs[i] = (np, new_size);
            }

            assert_valid_pointers(&ptrs);
        }

        deinit(m);
    }
}
