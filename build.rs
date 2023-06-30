extern crate cmake;

/// Determines if the current compilation mode is "debug".
fn is_debug() -> bool {
    if let Ok(opt_level) = std::env::var("OPT_LEVEL") {
        opt_level != "3"
    } else {
        println!("cargo:warning=`OPT_LEVEL` environment variable not found");
        true
    }
}

/// Hints cargo about the rerun rules to apply.
fn send_rerun_rules() {
    println!("cargo:rerun-if-changed=bartleby/");
    println!("cargo:rerun-if-changed=CMakeLists.txt");
    println!("cargo:rerun-if-env-changed=OPT_LEVEL");
    println!("cargo:rerun-if-env-changed=LLVM_DIR");
    println!("cargo:rerun-if-env-changed=build.rs");
}

fn main() {
    send_rerun_rules();

    let llvm_dir = std::env::var("LLVM_DIR").expect("`LLVM_DIR`");

    let dst = cmake::Config::new(".")
        .define(
            "CMAKE_BUILD_TYPE",
            if is_debug() { "Debug" } else { "Release" },
        )
        .define("LLVM_DIR", llvm_dir)
        .build_arg(format!(
            "-j{}",
            std::env::var("NUM_JOBS").unwrap_or("1".into())
        ))
        .build();
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=Bartleby");
    println!("cargo:rustc-link-lib=static=Bartleby-c");
}
