## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard()

include(list_subdirectories)

# If the current platform is WIN32, out_target will have ".exe" appended to it and the result
# stored in out_target.
function(nx_go_fix_target_exe target out_target)
    if(WIN32)
        set(${out_target} "${target}.exe" PARENT_SCOPE)
    else()
        set(${out_target} ${target} PARENT_SCOPE)
    endif()
endfunction()
