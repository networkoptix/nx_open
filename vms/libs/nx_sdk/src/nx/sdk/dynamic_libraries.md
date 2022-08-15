# Using dynamic libraries in Plugins

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This document describes issues which may arise when a Plugin dynamic library depends on any other
dynamic libraries, either taken from the OS (Windows or Linux), or coming with the Plugin in its
dedicated directory.

NOTE: The issues described below do not take place if the Plugin manually loads a dynamic library
via `LoadLibrary()` on Windows, or `dlopen()` on Linux.

The consequences of not following the recommendations below can vary from Plugin loading failures
to VMS Server crashes. And even if the Plugin works well with the particular VMS version on a
particular OS version, the failures may appear when VMS or OS is upgraded.

---------------------------------------------------------------------------------------------------
## Depending on libraries bundled with the VMS

A properly written Plugin is expected to remain compatible with any future version of the VMS,
including minor updates like monthly patches. So, if the Plugin is dynamically linked to any of the
libraries which come with the VMS installation, it may malfunction with any VMS version which has
any changes in those libraries. Such changes are expected to appear frequently. This issue may
happen regardless of whether the library referenced by the Plugin is an integral part of the VMS
code, or is a third-party library which the VMS depends on.

SOLUTION: The Plugin should never depend on any dynamic library bundled with the VMS installation.

---------------------------------------------------------------------------------------------------
## Depending on system libraries

Using any dynamic library from the OS by a Plugin is unsafe. The reason for that is the limitation
of the dynamic library concept in Windows and Linux-like systems, described below.

When a library (in this case a Plugin) is being loaded, and this library imports (requires) some
symbol (a function or global variable) from another library, there is no direct control of which
physical library will this particular symbol be taken from. The Plugin author can only specify
which libraries the Plugin depends on, and (separately from that) which symbols the Plugin requires
(without attributing them to libraries). When the Plugin is being loaded, each required symbol
will first be looked up in the libraries already loaded into the process. If it happens to be found
in a library which is bundled with the VMS, it will be taken from there, even if the plugin
library states that it depends on another library (e.g. another version of the same library) which
also contains such symbol.

EXAMPLE: The Plugin depends on `libxxx.so` located in the OS, and works great with the VMS version
A. But in the VMS version B it has been decided to bundle VMS with `libxxx.so`, possibly having
some internal differences to the Plugin's `libxxx.so`. So, after the VMS upgrade from A to B, this
Plugin will automatically start working with VMS'es `libxxx.so` instead of the OS'es one, and this
cannot be changed. Note that this happens in both cases: `libxxx.so` is the Linux system library,
or it comes bundled with the Plugin in its directory.

SOLUTION: The Plugin should be linked to system libraries statically, except for libraries which
are never bundled with the VMS, are designed to be backwards-compatible, and not likely to be
patched for the need of a certain project. An example of such a safe-to-depend-on library is
`libc` (but not `libstdc++`, see below).

ATTENTION: It means that `libstdc++` - the C++ standard library implementation - should be linked
statically, because VMS may decide to use a different version, probably coming from a different
toolchain.

---------------------------------------------------------------------------------------------------
## Bundling public libraries with the Plugin

NOTE: Here we discuss bundling the Plugin with publicly available dynamic libraries; bundling the
Plugin with dynamic libraries written by the Plugin vendor is fine, provided that they use some
unique symbol naming scheme, e.g. vendor-specific namespaces or prefixes.

There is no way for a single process to be dynamically linked with several libraries containing
identical symbols, so that some parts of the process use these symbols from one library, and some
other parts - from another library. So, if VMS is using a certain library A, regardless of whether
it is a Linux system library or it is bundled with VMS, the plugin physically cannot use any other
library (regardless of whether it has the same name A) which would provide the same symbols.

EXAMPLE: The Server uses `libssl.so.1.0.0.` The Plugin author decided to modify SSL library sources
and make a dynamic library out of them, having the symbols (function names) in it unchanged, and
bundles this library with the Plugin. It will not work as expected - when the Plugin is loaded, its
code will be tuned by the Linux library loader to use functions from the `libssl.so.1.0.0` used by
the Server (and already loaded into the process), and the modified library will not be loaded.
This takes place regardless of whether the modified SSL library is called `libssl.so.1.0.0` or
differently - only the symbols (funciton names) matter.

SOLUTION: The Plugin should be linked to public libraries statically.
