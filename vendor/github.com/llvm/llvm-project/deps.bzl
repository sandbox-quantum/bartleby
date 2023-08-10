load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@llvm-project.llvm//utils/bazel:zlib.bzl", "llvm_zlib_external")
load("@llvm-project.llvm//utils/bazel:terminfo.bzl", "llvm_terminfo_disable")

def llvm_deps():
    # Disable `terminfo` in LLVM builds.
    maybe(
        llvm_terminfo_disable,
        name = "llvm_terminfo",
    )

    # Enable `zlib` in LLVM builds (needed by LLVM profile tools)
    maybe(
        llvm_zlib_external,
        name = "llvm_zlib",
        external_zlib = "@com_sandboxquantum_bartleby//vendor/github.com/madler/zlib",
    )
