load("@bartleby-llvm-project.llvm//utils/bazel:configure.bzl", "llvm_configure")
load("//vendor/github.com/llvm/llvm-project:deps.bzl", "llvm_deps")
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

def bartleby_deps():
    """Initialize bartleby dependencies"""
    rules_foreign_cc_dependencies()

    llvm_configure(name = "bartleby-llvm-project")
    llvm_deps()
