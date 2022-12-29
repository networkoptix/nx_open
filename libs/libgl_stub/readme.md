# OpenGL Stub

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This library is just a stub for libGL.so which is required by libQt5Gui in Linuxes where we provide
Desktop Client in addition to Mediaserver. While it's OK for the Client to require libGL, it's
really weird when a console app (like Media Server) requires it and some piece of Xorg in addition.
Since non-GUI apps do not use OpenGL facilities of Qt, it's safe to provide a dummy library which
contains all the necessary symbols and thus makes `ld` happy without the real libGL.
