load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "1.2.13"
_SHA256SUM = "b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30"
_RELEASE_DATE = "Oct 14 2022"

def fetch_archive():
    maybe(
        http_archive,
        name = "madler.zlib",
        urls = ["https://github.com/madler/zlib/releases/download/v1.2.13/zlib-{version}.tar.gz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        strip_prefix = "zlib-{version}".format(version = _VERSION),
        build_file_content = """filegroup(name = "all_srcs", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
    )
