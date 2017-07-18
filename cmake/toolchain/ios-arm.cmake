set(IOS_PLATFORM OS)
set(IOS_DEPLOYMENT_TARGET 8.0)
set(ENABLE_BITCODE OFF)
include("${CMAKE_SOURCE_DIR}/cmake/toolchain/ios-cmake/ios.toolchain.cmake")
