# LibContext and RefCountableRegistry mechanisms

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This document describes the SDK's "hidden" (working automatically) mechanism called LibContext.

Each dynamic library which encorporates nx/sdk/helpers/lib_context.cpp, such as a Plugin's dynamic
library and the main dynamic library of the Server, owns its instance of nx::sdk::ILibContext
(`src/nx/sdk/lib_context.h`). This instance is owned in a static variable in a local (non-exported)
function `libContext()` defined in `src/nx/sdk/lib_context.cpp`.

The Server can access the LibContext instance of each Plugin, because that .cpp also exports a
C-style function `ILibContext* nxLibContext()` which returns a pointer to the LibContext instance.
After loading a Plugin, the Server creates a dedicated instance of
`nx::sdk::IRefCountableRegistry`, and supplies it to this instance of LibContext. Also, the Server
creates an instance of the registry for its own use, and supplies it to its own LibContext. The
implementation of `nx::sdk::IRefCountableRegistry` resides in the Server and is not part of the
SDK.

The interface `nx::sdk::IRefCountableRegistry` offers just two methods: notify about object
creation, and object deletion. The class `nx::sdk::RefCountable` (the recommended base class for
all ref-countable objects) calls them before attempting the respective operations, taking the
RefCountableRegistry instance from the LibContext instance of the current library.

For more details, see the Doxygen comments in the above mentioned source code files.
