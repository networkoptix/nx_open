include_guard(GLOBAL)

set(signingServer "http://localhost:8080" CACHE STRING "Signing server address")

if(customization.id)
    # Customization may forecefully disable the code signing.
    message(FATAL_ERROR "Variable must be defined before customization is loaded")
endif()

# Enable code signing for all publication types except local (developer one).
set(codeSigning true)
if(publicationType STREQUAL "local")
    set(codeSigning false)
endif()
message(STATUS "Code signing is ${codeSigning} for the ${publicationType} publication type")

if(NOT build_utils_dir)
    set(build_utils_dir "${CMAKE_SOURCE_DIR}/build_utils")
    message(STATUS "Build utils directory ${build_utils_dir}")
endif()
