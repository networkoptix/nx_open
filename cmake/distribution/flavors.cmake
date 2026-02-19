## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

# Definition of existing flavors - a list for each targetDevice. Variable should be named as
# `existing_flavors_${targetDevice}`. If it does not exist, only "default" flavor is allowed.
set(existing_flavors_linux_arm64
    "default"
    "axis_acap"
    "hanwha_edge1"
    "vca_edge1"
    "vivotek_edge1"
    "milesight_edge1"
    "rockchip_nvr1")

set(_flavorsList "default")

# Customization package contains list of linux_arm64 flavors (except the default one). If some
# flavors are enabled, the corresponding CMake variable will contain a list of strings. Otherwise
# the variable can be absent or contain "[]" string, depending on the CMS behavior.
if("${targetDevice}" STREQUAL "linux_arm64")
    # Build all flavors on CI pipelines to ensure their consistency and integrity.
    if("${publicationType}" STREQUAL "local")
        set(_flavorsList ${existing_flavors_linux_arm64})
    # For the release builds flavors should be loaded from the customization package.
    elseif(DEFINED customization.desktop.flavors AND NOT "${customization.desktop.flavors}" STREQUAL "[]")
        list(APPEND _flavorsList ${customization.desktop.flavors})
    endif()
endif()
list(JOIN _flavorsList "," _flavors)
unset(_flavorsList)

set(flavors ${_flavors} CACHE STRING "Comma-separated list of flavors (distribution package formats).")

unset(_flavors)

# Validates the value of the configuration variable `flavors`, and initializes the
# distribution_flavor_list variable accordingly.
#
# Args: none
#
# Output:
#  - distribution_flavor_list: global variable containing the list of flavors to build.
function(nx_init_distribution_flavor_list)
    set(existing_flavor_list ${existing_flavors_${targetDevice}})
    if(NOT existing_flavor_list)
        # NOTE: Each targetDevice always has a `default` flavor.
        set(existing_flavor_list "default")
    endif()

    string(REPLACE "," ";" distribution_flavor_list "${flavors}")

    list(JOIN existing_flavor_list ", " existing_flavor_string)

    foreach(flavor ${distribution_flavor_list})
        if(NOT ${flavor} IN_LIST existing_flavor_list)
            list(REMOVE_ITEM distribution_flavor_list ${flavor})
            message(WARNING
                "\nBad flavor \"${flavor}\" for ${targetDevice}. Valid flavors are: "
                "${existing_flavor_string}\n"
            )
        endif()
    endforeach()

    if(distribution_flavor_list STREQUAL "")
        message(FATAL_ERROR
            "\nNo valid flavors specified. Check the value of the `flavors` cached variable; "
            "valid flavors for ${targetDevice} are: ${existing_flavor_string}\n"
        )
    endif()

    set(distribution_flavor_list ${distribution_flavor_list} PARENT_SCOPE)
endfunction()

# Calls cmake's add_subdirectory() for the subdirectory of the "distribution/" directory passed as
# an argument, and all its subdirectories which are needed to build the "flavored" distributions.
# Only flavors from distribution_flavor_list are added.
#
# The expected directory structure is:
# <distribution_subdirectory>/
#     CMakeLists.txt
#     <files and directories needed to build the default flavor>
#     flavors/
#         <flavor_1>/
#             CMakeLists.txt
#             <files and directories needed to build flavor 1>
#         ...
#         <flavor_n>/
#             CMakeLists.txt
#             <files and directories needed to build flavor n>
#
# The special case is when <distribution_subdirectory> is set to "distribution/flavor_specific/" -
# it does not contain the default flavor, so only its subdirectories are added but not the
# directory itself.
#
# Args:
#  - distribution_subdirectory: package type subdirectory of the "distribution" directory.
function(nx_add_subdirectories_for_flavors distribution_subdirectory)
    foreach(flavor ${distribution_flavor_list})
        # NOTE: The `flavor` variable can be used in the added subdirectory.

        if(flavor STREQUAL "default") #< "default" is a special case.
            set(flavor_dir ${CMAKE_CURRENT_SOURCE_DIR}/${distribution_subdirectory})
        else()
            set(flavor_dir
                ${CMAKE_CURRENT_SOURCE_DIR}/${distribution_subdirectory}/flavors/${flavor})
        endif()
        if(EXISTS "${flavor_dir}/CMakeLists.txt")
            add_subdirectory(${flavor_dir})
        endif()
    endforeach()
endfunction()
