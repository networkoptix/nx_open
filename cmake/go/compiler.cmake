## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard()

set(NX_GO_COMPILER "${CONAN_GO_ROOT}/bin/go")

# Get the environment with which to invoke a go command through cmake -E env. i.e:
#
# cmake_go_env(go_env)
# execute_process(COMMAND ${CMAKE_COMMAND} -E env ${go_env} go test ./...
function(cmake_go_env out_go_env)
    set(goenv GOTMPDIR=${CMAKE_CURRENT_BINARY_DIR})
    list(APPEND goenv GOPROXY=direct)
    if(WIN32)
        list(APPEND goenv --modify PATH=path_list_append:${CONAN_MINGW-W64_ROOT}/bin)
    endif()

    set(${out_go_env} ${goenv} PARENT_SCOPE)
endfunction()

# Get a list of the form: COMMAND ${CMAKE_COMMAND} -E env <cmake_go_env> ${NX_GO_COMPILER}
# The result can be used as the input for a call to execute_process or add_custom_target etc.
# this is a prefix for a more specific go command. Append arguments as needed, e.g:
#
# get_go_cmake_command(go)
#
# execute_process(${go} build -c <directory-to-build> -o <output-path>)
#
# add_custom_target(
#   <target>
#   ${go} test -c <directory-to-build> -o <output-path>)

function (get_go_cmake_command out_var)
    cmake_go_env(goenv)
    set(full_cmd COMMAND ${CMAKE_COMMAND} -E env ${goenv} ${NX_GO_COMPILER})

    set(${out_var} ${full_cmd} PARENT_SCOPE)
endfunction()

function(go_fetch_module_dependencies)
    cmake_parse_arguments(ARG "TEST" "WORKING_DIRECTORY" "" ${ARGN})

    if(NOT ARG_WORKING_DIRECTORY)
        set(ARG_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    set(flags)

    if(ARG_TEST)
        list(APPEND flags "-test")
    endif()

    message("go: fetching dependencies for module: ${ARG_WORKING_DIRECTORY}")

    get_go_cmake_command(cmake_go)
    execute_process(
        ${cmake_go} list ${flags} ./...
        WORKING_DIRECTORY ${ARG_WORKING_DIRECTORY}
    )
endfunction()
