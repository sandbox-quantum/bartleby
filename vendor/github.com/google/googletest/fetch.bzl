load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "1.13.0"
_SHA256SUM = "ad7fdba11ea011c1d925b3289cf4af2c66a352e18d4c7264392fead75e919363"
_RELEASE_DATE = "Jan 17 2023"

def fetch_archive():
    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/refs/tags/v{version}.tar.gz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        strip_prefix = "googletest-{version}".format(version = _VERSION),
    )
