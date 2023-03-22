load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "1.4.1"
_SHA256SUM = "060426b186670beede4104095324a72bd7494d8b4e785bf0d84a612978285908"
_RELEASE_DATE = "Feb 9 2023"

def fetch_archive():
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = _SHA256SUM,
        strip_prefix = "bazel-skylib-{version}".format(version = _VERSION),
        url = "https://github.com/bazelbuild/bazel-skylib/archive/{version}.tar.gz".format(version = _VERSION),
    )
