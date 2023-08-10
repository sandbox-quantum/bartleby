load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

_VERSION = "16.0.6"
_SHA256SUM = "56b2f75fdaa95ad5e477a246d3f0d164964ab066b4619a01836ef08e475ec9d5"
_RELEASE_DATE = "Jun 14 2023"

def fetch_archive():
    maybe(
        http_archive,
        name = "llvm-project.llvm",
        urls = ["https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-{version}.tar.gz".format(version = _VERSION)],
        sha256 = _SHA256SUM,
        patch_args = ["-p1"],
        patches = [
            "@com_sandboxquantum_bartleby//vendor/github.com/llvm/llvm-project:fix_blake3_missing_header_and_layering_check.patch",
        ],
        strip_prefix = "llvm-project-llvmorg-{version}".format(version = _VERSION),
        build_file_content = """filegroup(name = "all_srcs", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
    )
