load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "15.0.7"
_SHA256SUM = "42a0088f148edcf6c770dfc780a7273014a9a89b66f357c761b4ca7c8dfa10ba"
_RELEASE_DATE = "Jan 12 2023"

def fetch_archive():
    maybe(
        http_archive,
        name = "llvm-project.llvm",
        urls = ["https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-{version}.tar.gz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        strip_prefix = "llvm-project-llvmorg-{version}".format(version = _VERSION),
        build_file_content = """filegroup(name = "all_srcs", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
    )
