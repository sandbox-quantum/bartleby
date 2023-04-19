load("//private:utils.bzl", "get_libraries_of")

def _bartleby_impl(ctx):
    """Implementation for rule `bartleby`."""
    target = ctx.attr.srcs

    libs = get_libraries_of(
        libs = ctx.attr.srcs,
        shared_only = False,
        static_only = True,
    )

    args = ctx.actions.args()

    out_name = "lib{}_bartleby.a".format(ctx.label.name)
    out = ctx.actions.declare_file(out_name)
    args.add("-o", out)

    if ctx.attr.prefix != None:
        args.add("--prefix", ctx.attr.prefix)

    for l in libs:
        args.add(l)

    ctx.actions.run(
        outputs = [out],
        inputs = libs,
        executable = ctx.executable._bartleby,
        arguments = [args],
    )

    user_link_flags = []
    cc_contexts = []
    for lib in ctx.attr.srcs:
        cc_contexts.append(lib[CcInfo].compilation_context)
        for li in lib[CcInfo].linking_context.linker_inputs.to_list():
            user_link_flags += li.user_link_flags

    cc_context = cc_common.merge_compilation_contexts(
        compilation_contexts = cc_contexts,
    )

    lib = cc_common.create_library_to_link(
        actions = ctx.actions,
        pic_static_library = out,
    )

    linker_input = cc_common.create_linker_input(
        owner = ctx.label,
        libraries = depset([lib]),
        user_link_flags = user_link_flags,
    )

    linking_context = cc_common.create_linking_context(
        linker_inputs = depset([linker_input]),
    )

    cc_info = CcInfo(
        compilation_context = cc_context,
        linking_context = linking_context,
    )

    return [
        DefaultInfo(files = depset([out])),
        cc_info,
    ]

"""Run bartleby on libraries."""
bartleby = rule(
    doc = """Run Bartleby on a set of libraries.

This outputs a target that provides a [`CcInfo`](https://bazel.build/rules/lib/CcInfo) provider.""",
    implementation = _bartleby_impl,
    attrs = {
        "srcs": attr.label_list(mandatory = True, doc = "Libraries to give to bartleby. These targets have to provide a [`CcInfo`](https://bazel.build/rules/lib/CcInfo) provider."),
        "prefix": attr.string(mandatory = False, doc = "Prefix to apply to library's symbols"),
        "_bartleby": attr.label(
            doc = "bartleby tool",
            executable = True,
            cfg = "exec",
            default = Label("//bartleby/tools/Bartleby:bartleby"),
        ),
    },
    fragments = ["cpp"],
)
