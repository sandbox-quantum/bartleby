load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Release date: Nov 6 2023
_VERSION = "1.5.0"
_SHA256SUM = "cd55a062e763b9349921f0f5db8c3933288dc8ba4f76dd9416aac68acee3cb94"

def fetch_archive():
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = _SHA256SUM,
        url = "https://github.com/bazelbuild/bazel-skylib/releases/download/{version}/bazel-skylib-{version}.tar.gz".format(version = _VERSION),
    )
