"""`LibraryToLink` attributes that define static libraries."""
_STATIC_LIBRARIES_ATTRS = ("static_library", "pic_static_library")

"""`LibraryToLink` attributes that define shared libraries."""
_SHARED_LIBRARIES_ATTRS = ("dynamic_library",)

def get_libraries_of(libs, shared_only = False, static_only = False):
    """Returns the files corresponding to cc libraries from a list of
    target."""
    if static_only and shared_only:
        fail("`static_only` and `shared_only` are mutually exclusive")

    attrs = []
    if not shared_only:
        attrs += list(_STATIC_LIBRARIES_ATTRS)
    if not static_only:
        attrs += list(_SHARED_LIBRARIES_ATTRS)

    outs = []
    for lib in libs:
        lc = lib[CcInfo].linking_context
        for linker_input in lc.linker_inputs.to_list():
            for library in linker_input.libraries:
                for a in attrs:
                    l = getattr(library, a)
                    if l and l not in outs:
                        outs.append(l)

    if len(outs) == 0:
        fail("no library found in {}", libs)

    return outs
