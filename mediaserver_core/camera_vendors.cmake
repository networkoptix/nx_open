if(NOT targetDevice MATCHES "android|ios|edge1")
    set(ENABLE_ONVIF true)
    set(ENABLE_AXIS true)
    set(ENABLE_ACTI true)
    set(ENABLE_ARECONT true)
    set(ENABLE_DLINK true)
    set(ENABLE_DROID true)
    set(ENABLE_TEST_CAMERA true)
    set(ENABLE_STARDOT true)
    set(ENABLE_IQE true)
    set(ENABLE_ISD true)
    set(ENABLE_PULSE_CAMERA true)
    set(ENABLE_FLIR true)
    set(ENABLE_ADVANTECH true)
    set(ENABLE_WEARABLE true)

    if(customization STREQUAL "hanwha")
        set(ENABLE_HANWHA true)
    endif()
endif()

# TODO: mediaserver_core uses MDNS unconditionally, so disabling this macro always leads to build
# failure.
if(NOT targetDevice MATCHES "android|ios")
    set(ENABLE_THIRD_PARTY true)
    set(ENABLE_MDNS true)
endif()

if(WINDOWS)
    set(ENABLE_VMAX true)
    set(ENABLE_DESKTOP_CAMERA true)
endif()

nx_configure_file(camera_vendors.h.in ${CMAKE_CURRENT_BINARY_DIR}/camera_vendors.h)

set(vendor_source_exclusions)

if(NOT ENABLE_HANWHA)
    list(APPEND vendor_source_exclusions src/plugins/resource/hanwha/)
endif()

# TODO: Remove this function and use camera_vendors.h where needed.
function(nx_add_vendor_macros target)
    if(NOT targetDevice MATCHES "android|ios|edge1")
        target_compile_definitions(${target} PUBLIC
            ENABLE_ONVIF
            ENABLE_AXIS
            ENABLE_ACTI
            ENABLE_ARECONT
            ENABLE_DLINK
            ENABLE_DROID
            ENABLE_TEST_CAMERA
            ENABLE_STARDOT
            ENABLE_IQE
            ENABLE_ISD
            ENABLE_PULSE_CAMERA
            ENABLE_FLIR
            ENABLE_ADVANTECH
            ENABLE_WEARABLE
        )
    endif()

    if(NOT targetDevice MATCHES "android|ios")
        target_compile_definitions(${target} PUBLIC
            ENABLE_THIRD_PARTY
            ENABLE_MDNS
        )
    endif()

    if(WINDOWS)
        target_compile_definitions(${target} PUBLIC
            ENABLE_VMAX
            ENABLE_DESKTOP_CAMERA
        )
    endif()
endfunction()
