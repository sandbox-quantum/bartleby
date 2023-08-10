load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "1.4.2"
_SHA256SUM = "66ffd9315665bfaafc96b52278f57c7e2dd09f5ede279ea6d39b2be471e7e3aa"
_RELEASE_DATE = "May 31 2023"

def fetch_archive():
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = _SHA256SUM,
        url = "https://github.com/bazelbuild/bazel-skylib/releases/download/{version}/bazel-skylib-{version}.tar.gz".format(version = _VERSION),
    )
