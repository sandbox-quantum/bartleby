load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "18.1.3"
_SHA256SUM ="2929f62d69dec0379e529eb632c40e15191e36f3bd58c2cb2df0413a0dc48651"
_RELEASE_DATE = "Apr 4 2024"

def fetch_archive():
    maybe(
        http_archive,
        name = "llvm-raw",
        urls = ["https://github.com/llvm/llvm-project/releases/download/llvmorg-{version}/llvm-project-{version}.src.tar.xz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        strip_prefix = "llvm-project-{version}.src".format(version = _VERSION),
        build_file_content = """filegroup(name = "all_srcs", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
    )
