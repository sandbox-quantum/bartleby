load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "18.1.2"
_SHA256SUM = "51073febd91d1f2c3b411d022695744bda322647e76e0b4eb1918229210c48d5"
_RELEASE_DATE = "Mar 30 2024"

def fetch_archive():
    maybe(
        http_archive,
        name = "llvm-raw",
        urls = ["https://github.com/llvm/llvm-project/releases/download/llvmorg-{version}/llvm-project-{version}.src.tar.xz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        strip_prefix = "llvm-project-{version}.src".format(version = _VERSION),
        build_file_content = """filegroup(name = "all_srcs", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
    )
