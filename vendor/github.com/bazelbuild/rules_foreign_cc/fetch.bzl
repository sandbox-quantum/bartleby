load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "0.9.0"
_SHA256SUM = "2a4d07cd64b0719b39a7c12218a3e507672b82a97b98c6a89d38565894cf7c51"
_RELEASE_DATE = "Aug 2 2022"

def fetch_archive():
    maybe(
        http_archive,
        name = "rules_foreign_cc",
        sha256 = _SHA256SUM,
        strip_prefix = "rules_foreign_cc-{version}".format(version = _VERSION),
        url = "https://github.com/bazelbuild/rules_foreign_cc/archive/{version}.tar.gz".format(version = _VERSION),
    )
