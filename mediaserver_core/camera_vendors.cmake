set(_enabled_camera_vendors)
set(_enabled_camera_vendors_old)

function(nx_enable_camera_vendor vendor)
    set(_enabled_camera_vendors ${_enabled_camera_vendors} ${vendor} PARENT_SCOPE)
endfunction()

# TODO: Refactor vendors to use new approach, then remove this function.
function(nx_enable_camera_vendor_old vendor)
    set(_enabled_camera_vendors_old ${_enabled_camera_vendors_old} ${vendor} PARENT_SCOPE)
endfunction()

function(nx_add_vendor_macros target)
    foreach(vendor ${camera_vendors})
        string(TOUPPER ${vendor} VENDOR)
        if(vendor IN_LIST _enabled_camera_vendors_old)
            target_compile_definitions(${target} PUBLIC ENABLE_${VENDOR})
        endif()
    endforeach()
endfunction()

#--------------------------------------------------------------------------------------------------
# Registering vendors

set(vendor_source_exclusions)

set(camera_vendors
    onvif
    axis
    acti
    arecont
    dlink
    droid
    test_camera
    stardot
    iqe
    isd
    pulse_camera
    flir
    advantech
    wearable
    hanwha
    third_party
    mdns
    vmax
    desktop_camera
)

if(NOT targetDevice MATCHES "android|ios|edge1")
    nx_enable_camera_vendor_old(onvif)
    nx_enable_camera_vendor_old(axis)
    nx_enable_camera_vendor_old(acti)
    nx_enable_camera_vendor_old(arecont)
    nx_enable_camera_vendor_old(dlink)
    nx_enable_camera_vendor_old(droid)
    nx_enable_camera_vendor_old(test_camera)
    nx_enable_camera_vendor_old(stardot)
    nx_enable_camera_vendor_old(iqe)
    nx_enable_camera_vendor_old(isd)
    nx_enable_camera_vendor_old(pulse_camera)
    nx_enable_camera_vendor_old(flir)
    nx_enable_camera_vendor_old(advantech)
    nx_enable_camera_vendor_old(wearable)

    if(customization STREQUAL "hanwha")
        nx_enable_camera_vendor(hanwha)
    endif()
endif()

# TODO: mediaserver_core uses MDNS unconditionally, so disabling this macro always leads to build
# failure.
if(NOT targetDevice MATCHES "android|ios")
    nx_enable_camera_vendor_old(third_party)
    nx_enable_camera_vendor_old(mdns)
endif()

if(WINDOWS)
    nx_enable_camera_vendor_old(vmax)
    nx_enable_camera_vendor_old(desktop_camera)
endif()

#--------------------------------------------------------------------------------------------------

set(camera_vendors_macros)

foreach(vendor ${camera_vendors})
    string(TOUPPER ${vendor} VENDOR)

    if(vendor IN_LIST _enabled_camera_vendors)
        string(APPEND camera_vendor_macros "#define ENABLE_${VENDOR}\n")
    elseif(vendor IN_LIST _enabled_camera_vendors_old)
        continue()
    else()
        list(APPEND vendor_source_exclusions src/plugins/resource/${vendor}/)
    endif()
endforeach()

nx_configure_file(camera_vendors.h.in ${CMAKE_CURRENT_BINARY_DIR}/camera_vendors.h)
