## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(TEST_APPS OFF CACHE BOOL "Build test applications for libsrtp2" FORCE)
set(ENABLE_OPENSSL ON CACHE BOOL "Enable OpenSSL crypto engine" FORCE)

add_subdirectory(
    ${open_source_root}/artifacts/libsrtp/src
    ${CMAKE_BINARY_DIR}/artifacts/libsrtp/src
)

# Add generated config.h file to the list of known files to prevent ninja_tool from deleting it.
nx_store_known_file(${CMAKE_BINARY_DIR}/artifacts/libsrtp/src/config.h)

set_target_properties(srtp2 PROPERTIES FOLDER third_party)
