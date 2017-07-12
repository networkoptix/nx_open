set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(bin_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources")
set(target_bin_path "${CMAKE_BINARY_DIR}/Release/bin")
set(filter_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources")
set(ClientHelpSourceDir "${target_bin_path}/help")
set(ClientVoxSourceDir "${target_bin_path}/vox")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${target_bin_path}/fonts")
set(VC14RedistPath "$ENV{environment}/packages/${rdep_target}/vcredist-2015")
#set(NxtoolQuickControlsDir ${libdir}\bin\${build.configuration}\qml\QtQuick\Controls)
#set(ClientQmlDir ${libdir}\bin\${build.configuration}\qml)

set(installers
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.server}.msi"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.server}.msi"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.server}.exe"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.client}.msi"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.client}.exe"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.bundle}.exe"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.servertool}.msi"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/${artifact.name.servertool}.exe"
)

set(wxs
    "Clientbg.wxs"
    "Clientfonts.wxs"
    "Clienthelp.wxs"
    "Clientqml.wxs"
    "Clientvcrt14.wxs"
    "Clientvox.wxs"
)

function (nx_copy_filter_resources input output)
    file(GLOB files RELATIVE ${input} ${input}/*)
        foreach (file ${files})
            set(srcTemplatePath ${input}/${file})
            if(NOT IS_DIRECTORY ${srcTemplatePath})
                  nx_configure_file(${input}/${file} ${output})
            endif(NOT IS_DIRECTORY ${srcTemplatePath})
        endforeach(file)

    file(GLOB files RELATIVE ${input} ${input}/**/*)
    foreach (file ${files})
        nx_configure_file(${input}/${file} ${output}/${file})
    endforeach(file)
endfunction()

nx_copy_bin_resources (${bin_resources} "${CMAKE_CURRENT_BINARY_DIR}")
nx_copy_filter_resources (${filter_resources} "${CMAKE_CURRENT_BINARY_DIR}")

execute_process(
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/build.bat
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/CustomActions
)

add_custom_command(
    OUTPUT ${wxs}
    COMMENT "++++++++++++++++++++++++++++++++++++++++++++ Generating wxs..."
    COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_custom_command(
    OUTPUT ${installers}
    COMMENT "++++++++++++++++++++++++++++++++++++++++++++ Processing wxs..."
    DEPENDS ${wxs}
    COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_custom_target(gen DEPENDS ${wxs})
add_custom_target(compile DEPENDS ${installers})

ADD_DEPENDENCIES(compile gen)