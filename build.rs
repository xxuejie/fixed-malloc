use cc::Build;

fn main() {
    println!("cargo:rerun-if-changed=./linear-malloc.c");
    println!("cargo:rerun-if-changed=./slab-malloc.c");
    println!("cargo:rerun-if-changed=./linear-malloc.h");
    println!("cargo:rerun-if-changed=./slab-malloc.h");
    println!("cargo:rerun-if-changed=./utils.h");
    println!("cargo:rerun-if-changed=./c-list.h");

    let mut build = Build::new();
    if cfg!(feature = "test-support") {
        build.flag("-DFM_TEST_SUPPORT").flag("-DFM_GUARDS");
    }
    if cfg!(feature = "manual-init") {
        build.flag("-DFM_MANUAL_INIT");
    }
    build
        .file("./linear-malloc.c")
        .file("./slab-malloc.c")
        .include(".")
        .static_flag(true)
        .flag("-O3")
        .flag("-g")
        .flag("-std=c99")
        .flag("-nostdlib")
        .flag("-Wall")
        .flag("-Werror")
        .flag("-Wextra")
        .flag("-fno-builtin-printf")
        .flag("-fno-builtin-memcmp")
        .flag("-fdata-sections")
        .flag("-ffunction-sections")
        .flag("-DFM_MEMORY_SIZE=655360")
        .flag("-DFM_DEBUG(...)=")
        .compile("fixed-malloc");
}
