## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This avoids Qt from interventing into the configuration process. By default Qt tries to setup
# multi-ABI build. However we do it in our own way, so we just disable it. Don't be confused
# by "ON" value. It DISABLES the feature.
set(vms-MultiAbiBuild ON CACHE INTERNAL "")

function(prepare_qtcreator_android_target target)
    set(options)
    set(oneValueArgs PACKAGE_SOURCE QML_ROOT_PATH)
    set(multiValueArgs EXTRA_LIBS QML_IMPORT_PATH)
    cmake_parse_arguments(ANDROID "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    string(JOIN "," extra_libs ${ANDROID_EXTRA_LIBS})
    string(JOIN "," qml_imports ${ANDROID_QML_IMPORT_PATH})
    set_target_properties(${target} PROPERTIES
        ANDROID_PACKAGE_SOURCE_DIR "${ANDROID_PACKAGE_SOURCE}"
        ANDROID_EXTRA_LIBS "${extra_libs}"
        QML_ROOT_PATH "${ANDROID_QML_ROOT_PATH}"
        QML_IMPORT_PATH "${qml_imports}"
    )
endfunction()

function(activate_qtcreator_android_target target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Target ${target} does not exist.")
    endif()

    get_target_property(ANDROID_PACKAGE_SOURCE_DIR ${target} ANDROID_PACKAGE_SOURCE_DIR)
    get_target_property(ANDROID_EXTRA_LIBS ${target} ANDROID_EXTRA_LIBS)
    get_target_property(QML_ROOT_PATH ${target} QML_ROOT_PATH)
    get_target_property(QML_IMPORT_PATH ${target} QML_IMPORT_PATH)

    set(ANDROID_PACKAGE_SOURCE_DIR ${ANDROID_PACKAGE_SOURCE_DIR} CACHE INTERNAL "")
    set(ANDROID_EXTRA_LIBS ${ANDROID_EXTRA_LIBS} CACHE INTERNAL "")
    set(QML_ROOT_PATH ${QML_ROOT_PATH} CACHE INTERNAL "")
    set(QML_IMPORT_PATH ${QML_IMPORT_PATH} CACHE INTERNAL "")
endfunction()
