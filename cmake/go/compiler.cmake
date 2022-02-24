## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard()

if(NOT NX_GO_COMPILER)
    set(NX_GO_COMPILER "${CONAN_GO_ROOT}/bin/go")
endif()
