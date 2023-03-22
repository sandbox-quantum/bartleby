load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "0.5.3"
_SHA256SUM = "3fd8fec4ddec3c670bd810904e2e33170bedfe12f90adf943508184be458c8bb"
_RELEASE_DATE = "Sep 30 2022"

def fetch_archive():
    maybe(
        http_archive,
        name = "io_bazel_stardoc",
        sha256 = _SHA256SUM,
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/stardoc/releases/download/{version}/stardoc-{version}.tar.gz".format(version = _VERSION),
            "https://github.com/bazelbuild/stardoc/releases/download/{version}/stardoc-{version}.tar.gz".format(version = _VERSION),
        ],
    )
