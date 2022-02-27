## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(concurrentqueue_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/concurrentqueue)

add_library(concurrentqueue INTERFACE)
target_include_directories(concurrentqueue INTERFACE "${concurrentqueue_dir}/src/concurrentqueue")
