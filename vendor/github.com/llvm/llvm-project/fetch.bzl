load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Release date: Jun 6 2024
_VERSION = "18.1.7"
_SHA256SUM = "b60df7cbe02cef2523f7357120fb0d46cbb443791cde3a5fb36b82c335c0afc9"

def fetch_archive():
    maybe(
        http_archive,
        name = "llvm-raw",
        urls = ["https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-{version}.tar.gz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        strip_prefix = "llvm-project-llvmorg-{version}".format(version = _VERSION),
        build_file_content = """filegroup(name = "all_srcs", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
    )
