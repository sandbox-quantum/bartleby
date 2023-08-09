load("@llvm-project.llvm//utils/bazel:configure.bzl", "llvm_configure")
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
load("//vendor/github.com/llvm/llvm-project:deps.bzl", "llvm_deps")
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

def bartleby_deps(configure_llvm = True):
    """Initialize bartleby dependencies.

    If `configure_llvm` is set to `False`, then bartleby will use the one in
    the current Bazel workspace.
    If may be useful to set `configure_llvm` to `False` if LLVM is already imported
    in your Bazel workspace.
    """
    rules_foreign_cc_dependencies()
    bazel_skylib_workspace()

    if configure_llvm:
        llvm_configure(name = "llvm-project")
        llvm_deps()
