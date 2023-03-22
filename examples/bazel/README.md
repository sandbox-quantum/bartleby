# Setup

To use Bartleby in your project, add the following to your `WORKSPACE` file:

``` starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

_COMMIT = "01f9b7dd86cf415b5591deff4ba759345cc0af36"
_SHA256 = "efaa650c4c96ee671a73406fe9a625783e8ec404b7e4bdbf49a3e36b440754f0"

http_archive(
    name = "com_sandboxquantum_bartleby",
    sha256 = _SHA256,
    strip_prefix = "bartleby-{commit}".format(commit = _COMMIT),
    urls = ["https://github.com/sandbox-quantum/bartleby/archive/{commit}.tar.gz".format(commit = _COMMIT)],
)

# Loads bartleby external repositories.
load("@com_sandboxquantum_bartleby//bartleby:repositories.bzl", "bartleby_repositories")

bartleby_repositories()

# Loads an initializes bartleby dependencies.
load("@com_sandboxquantum_bartleby//bartleby:deps.bzl", "bartleby_deps")

bartleby_deps()
```

## Example

An example can be found in [`WORKSPACE.bazel`] and [`BUILD.bazel`].

[`WORKSPACE.bazel`]: /examples/bazel/WORKSPACE.bazel
[`BUILD.bazel`]: /examples/bazel/BUILD.bazel

## Bartleby Bazel rule.

See [rules.md](/docs/rules.md).