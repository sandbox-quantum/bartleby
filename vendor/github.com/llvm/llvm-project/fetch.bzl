load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "17.0.6"
_SHA256SUM = "58a8818c60e6627064f312dbf46c02d9949956558340938b71cf731ad8bc0813"
_RELEASE_DATE = "Jan 12 2023"

def fetch_archive():
    maybe(
        http_archive,
        name = "llvm-raw",
        urls = ["https://github.com/llvm/llvm-project/releases/download/llvmorg-{version}/llvm-project-{version}.src.tar.xz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        strip_prefix = "llvm-project-{version}.src".format(version = _VERSION),
        build_file_content = """filegroup(name = "all_srcs", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
    )
