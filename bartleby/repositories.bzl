load("//vendor/github.com/bazelbuild/rules_foreign_cc:fetch.bzl", rules_foreign_cc_fetch = "fetch_archive")
load("//vendor/github.com/google/googletest:fetch.bzl", googletest_fetch = "fetch_archive")
load("//vendor/github.com/llvm/llvm-project:fetch.bzl", llvm_fetch = "fetch_archive")
load("//vendor/github.com/madler/zlib:fetch.bzl", zlib_fetch = "fetch_archive")

def bartleby_repositories():
    """Declares workspaces Bartleby depend on. If you want to use Bartleby in your
    project, then this function should be called."""

    rules_foreign_cc_fetch()
    zlib_fetch()
    llvm_fetch()
    googletest_fetch()
