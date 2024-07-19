# Using dynamic libraries in Plugins

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This document describes issues which may arise when a Plugin dynamic library depends on any other
dynamic libraries, either taken from the OS (Windows or Linux), or coming with the Plugin in its
dedicated directory.

The consequences of not following the recommendations below can vary from Plugin loading failures
to VMS Server crashes. And even if the Plugin works well with the particular VMS version on a
particular OS version, the failures may appear when VMS or OS is upgraded.

The root cause of these issues is the fundamental deficiency of the OS mechanisms which resolve the
symbols after loading a dynamic library.

Note that if a plugin is statically linked to certain libraries, it does not guarantee that symbols
from that libraries are not present in the imported/exported symbol table in the plugin dynamic
library.

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
symbol (a function or a global variable) from another library, there is no direct control of which
physical library will this particular symbol be taken from. The Plugin author can only specify
which libraries the Plugin depends on, and (separately from that) which symbols the Plugin requires
(without attributing them to libraries). When the Plugin is being loaded, each required symbol
will first be looked up in the libraries already loaded into the process. If it happens to be found
in a library which is bundled with the VMS, it will be taken from there, even if the plugin
library specifies that it depends on another library (e.g. another version of the same library)
which also contains such symbol.

EXAMPLE: The Plugin depends on `libxxx.so` located in the OS, and works great with the VMS version
A. But in the VMS version B it has been decided to bundle the VMS with `libxxx.so`, possibly having
some internal differences to the Plugin's `libxxx.so`. So, after the VMS upgrade from A to B, this
Plugin will automatically start working with VMS'es `libxxx.so` instead of the OS'es one, and this
cannot be changed. Note that this happens in both cases: `libxxx.so` is the Linux system library,
or it comes bundled with the Plugin in its directory.

SOLUTION: The Plugin should be linked to system libraries statically, except for `libc` (but not
`libstdc++`, see below) which is never bundled with the VMS, is designed to be
backwards-compatible, and will not be patched for the need of a certain project.

---------------------------------------------------------------------------------------------------
## Depending on libstdc++ on Linux

`libstdc++` - the GCC C++ standard library implementation - should be linked dynamically. There is
a known issue in GCC's `libstdc++` - when linked statically, if some part of the product (in our
case it will be the VMS Server) links to this library dynamically, the input/output streams like
`std::cerr` will be initialized incorrectly, which may lead to output disruption and/or crashes.

This library must not be bundled with the plugin, because the one coming with the Server will be
used anyway, due to the symbol resolution mechanism.

Note that the plugin must use the version of `libstdc++` compatible with the one of the Server with
which the plugin is supposed to work. So, it is theoretically possible that at some point in the
future the Server will be upgraded to the one using some newer incompatible `libstdc++`, and so the
plugin will malfunction, including being unable to load or crashing.

SOLUTION: For the plugin, use Clang together with its native `libc++` instead of `libstdc++`, and
link to `libc++` statically to make sure that the plugin will function properly even if the Server
will start using `libc++` at some point in the future.

---------------------------------------------------------------------------------------------------
## Depending on Visual C++ runtime on Windows

When using Visual Studio to compile a plugin on Windows, and choosing to use dynamic linking with
the Visual C++ Runtime Redistributable (msvcp140.dll), the resulting plugin may be not compatible
with the VMS built using the toolsets (compilers) that use an older runtime version. The newer
runtime version will do fine.

For example, as of 2024-07-19, the std::mutex ABI has undergone a breaking change in the toolset
version 14.40 as opposed to 14.38, which did not happen for years before that. And now if a plugin
uses std::mutex, is compiled with 14.40, and is used in VMS compiled with 14.38, it will crash.
More details on this issue can be found here:
[Visual Studio forum](https://developercommunity.visualstudio.com/t/Access-violation-with-std::mutex::lock-a/10664660)

Currently, the recommendation is either to use the toolset version not newer than 14.38, or to link
the plugin with the C++ runtime statically.

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
