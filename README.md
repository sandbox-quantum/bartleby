# Bartleby: a symbol renaming tool.

  * [Why?](#why)
  * [How?](#how)
    * [Example](#how-example)
  * [Getting Started](#getting-started)
    * [Using CMake](#using-cmake)
    * [Using Bazel](#using-bazel)
    * [Importing Bartleby in your Bazel project](#bazel-import)
  * [License](#license)

This repository contains the source code for Bartleby, a library and a tool
to rename symbols across several object files and static libraries.

## Why? <a name="why"></a>

If you develop a library, your end product will contain:

  * definition for symbols that are made available to your users
    ("exported" functions and so on),
  * definition for symbols that you consider to be implementation details of
    your library and wish to hide from your users ("private" functions),
  * references to symbols that you use in your dependencies (and their
    definition if you link statically).

Bartleby helps you ship a static library exposing only the symbols you want
exposed and without polluting the namespace with the symbols of your own
dependencies.
It is similar to `objcopy --prefix-symbols=__private` but with
"intelligent" (see the ["How?"](#how) section below) automated selection of
which symbols to prefix (for instance a reference to puts won't be
renamed `private_puts` because it would lead to unresolved symbols).

This helps solving the diamond dependency problem: if a user of your library
wants to link against OpenSSL, which also happens to be a library you link
against, how do you make sure that the user remains in control of the
version of OpenSSL they use without having name collisions with your own use of
OpenSSL? You use Bartleby.

## How? <a name="how"></a>

Bartleby takes a set of files as input. These can be static libraries (`.a`) or
objects (`.o`). Bartleby figures out which symbols are considered private or not
by iterating over all these objects (the ones directly supplied by the user, or
the ones contained in the archives), and collects the visibility and the
definedness of each symbols found. Using these pieces of information, a map of
symbols is built as follows:

  * If a symbol is found in several places across the input files, and at least
    one object actually defines it, then the symbol is marked as defined.
  * If a symbol found in an object has global visibility, then the symbol is
    marked as global.

Finally, for each symbol marked as defined and global, Bartleby considers them
as private and adds a prefix to its name, and rename all its references across
all the objects.
The output is a single static library that contains all the processed objects.

### Example <a name="how-example"></a>

Let's say we have the following three C files, along with their header:

  * `api_1.c`:

```c
#include <stdio.h>

#include <external_dep.h>

__attribute__((visibility("default"))) void my_api(void) {
  puts("my_api called, calling external dep");
  char buf[0x41];
  external_api(buf, sizeof(buf));
}

void internal_impl(void) {
  puts("internal implementation, calling `my_api`");
  my_api();
}
```

  * `api_2.c`

```c
#include <stdio.h>

#include "api_1.h"

__attribute__((visibility("default"))) void another_api(void) {
  puts("another_api called, calling my_api");
  my_api();
}
```

  * `external_dep.c`

```c
#include <stdio.h>
#include <string.h>

__attribute__((visibility("default"))) void external_api(void *src, const size_t n) {
  puts("external_api called, calling external private impl");
  memset(src, 0, n);
}

void external_private_impl(void) {
  puts("external private implementation called");
}
```

We compile the first two objects from our API:

```shell
$ clang -fvisibility=hidden -c api_1.c api_2.c -isystem.
$ file api_1.o api_2.o
api_1.o: Mach-O 64-bit object arm64
api_2.o: Mach-O 64-bit object arm64
```

Then, we create a static library out of them:

```shell
$ ar rvs libapi.a api_1.o api_2.o
```

Now, let's build our simple external dependency:

```shell
$ clang -fvisibility=hidden -c external_dep.c
$ ar rvs libexternal.a external_dep.o
```

`libapi.a` is the library we want to have its symbols prefixed with our custom
prefix.
`libexternal.a` is one of our external dependencies, and we want the user to be
able to link against `libapi.a` without encountering any C symbol collisions
because `libexternal` might be used in another place.
We run Bartleby by giving `libapi.a` and `libexternal.a` as an input.
The output will be `libapi_v1.1.a`:

```shell
$ bartleby --if libapi.a \
           --if libexternal.a \
           --of libapi_v1.1.a \
           --prefix __impl_v1.1_
5 symbol(s) prefixed
libapi_v1.1.a produced.
```

Now, we use `nm` to inspect the produced archive:

```shell
$ nm libapi_v1.1.a
pi_1.o:
             	U ___impl_v1.1_external_api
0000000000000064 T ___impl_v1.1_internal_impl
0000000000000000 T ___impl_v1.1_my_api
             	U ___stack_chk_fail
             	U ___stack_chk_guard
             	U _puts
0000000000000084 r l_.str
00000000000000a8 r l_.str.1

api_2.o:
0000000000000000 T ___impl_v1.1_another_api
             	U ___impl_v1.1_my_api
             	U _puts
0000000000000020 r l_.str

external_dep.o:
0000000000000000 T ___impl_v1.1_external_api
000000000000001c T ___impl_v1.1_external_private_impl
             	U _puts
0000000000000038 r l_.str
000000000000006b r l_.str.1
```

We can now see that defined symbols have been prefixed with `__impl_v1.1`.
However, `puts` hasn’t been prefixed, because even if it’s a global symbol,
no definition has been found in either `libapi.a` or `libexternal.a`, therefore
Bartleby didn’t prefix it.

## Getting Started <a name="getting-started"></a>

Bartleby can be compiled with Bazel or CMake.

### Using CMake <a name="using-cmake"></a>

LLVM `>=` 15.0 is required to build Bartleby. See [Getting Started](https://llvm.org/docs/GettingStarted.html)
to find more information about how to build LLVM, and
[LLVM releases](https://releases.llvm.org/) to find all the releases.
APT packages are also available on the [LLVM Debian/Ubuntu packages page](https://apt.llvm.org/).

```shell
$ cmake -B build -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=/path/to/llvm-15/lib/cmake
$ cmake --build build
$ ./build/bin/bartleby
```

### Using Bazel <a name="using-bazel"></a>

It is highly recommended to use [`bazelisk`].

```shell
$ bazelisk build -c opt bartleby/...
$ ./bazel-bin/bartleby/tools/Bartleby/bartleby
```

### Importing Bartleby in your Bazel project <a name="bazel-import"></a>

It is also possible to import Bartleby in an existing Bazel workspace using
[`http_archive`].
One can use it as a tool, or can also use it through the [bartleby starlark
rule].
See [`examples/bazel`](/examples/bazel) for more information.


## License <a name="license"></a>

See [`LICENSE`].

[`bazelisk`]: https://github.com/bazelbuild/bazelisk/releases
[`http_archive`]: https://bazel.build/rules/lib/repo/http#http_archive
[bartleby starlark rule]: /docs/rules.md
[`LICENSE`]: /LICENSE