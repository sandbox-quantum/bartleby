load("//vendor/github.com/bazelbuild/stardoc:fetch.bzl", bazel_stardoc_fetch = "fetch_archive")

def fetch_archives():
    """Fetches all the archives required for building Bartleby."""
    bazel_stardoc_fetch()
