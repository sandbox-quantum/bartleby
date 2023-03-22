<!-- Generated with Stardoc: http://skydoc.bazel.build -->



<a id="bartleby"></a>

## bartleby

<pre>
bartleby(<a href="#bartleby-name">name</a>, <a href="#bartleby-prefix">prefix</a>, <a href="#bartleby-srcs">srcs</a>)
</pre>

Run Bartleby on a set of libraries.

This outputs a target that provides a [`CcInfo`](https://bazel.build/rules/lib/CcInfo) provider.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="bartleby-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="bartleby-prefix"></a>prefix |  Prefix to apply to library's symbols   | String | optional | <code>""</code> |
| <a id="bartleby-srcs"></a>srcs |  Libraries to give to bartleby. These targets have to provide a [<code>CcInfo</code>](https://bazel.build/rules/lib/CcInfo) provider.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |


