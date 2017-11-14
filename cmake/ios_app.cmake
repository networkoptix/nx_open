set(loginKeychainPassword "qweasd123" CACHE STRING "Password to unlock login keychain.")

function(add_ios_ipa target)
    cmake_parse_arguments(IPA "" "TARGET;FILE_NAME" "" ${ARGN})

    set(payload_dir "${CMAKE_CURRENT_BINARY_DIR}/ipa/$<CONFIGURATION>/Payload/${IPA_TARGET}.app")
    set(app_dir "$<TARGET_FILE_DIR:${IPA_TARGET}>")

    add_custom_target(${target} ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${payload_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${payload_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${app_dir}" "${payload_dir}"
        COMMAND
            cd "ipa/$<CONFIGURATION>"
                && ${CMAKE_COMMAND} -E tar cfv "${IPA_FILE_NAME}" --format=zip "Payload"
        DEPENDS ${IPA_TARGET}
        BYPRODUCTS "${IPA_FILE_NAME}"
        VERBATIM
        COMMENT "Creating ${IPA_FILE_NAME}"
    )
endfunction()

function(setup_ios_application target)
    cmake_parse_arguments(APP "" "QML_ROOT" "ADDITIONAL_PLUGINS" ${ARGN})

    if(NOT APP_QML_ROOT)
        message(FATAL_ERROR "QML_ROOT is not specified.")
    endif()

    set(plugins ${APP_ADDITIONAL_PLUGINS} qios)

    if(qml_debug)
        list(APPEND plugins
            qmldbg_debugger
            qmldbg_inspector
            qmldbg_local
            qmldbg_native
            qmldbg_profiler
            qmldbg_server
            qmldbg_tcp
        )
    endif()

    set(static_plugins_args_file "${CMAKE_CURRENT_BINARY_DIR}/${target}_static_plugins.parameters")

    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/build_utils/python/qmldeploy.py"
            --print-static-plugins
            --qt-root "${QT_DIR}" --qml-root "${APP_QML_ROOT}"
            --additional-plugins ${plugins}
            --output "${static_plugins_args_file}.copy"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot get list of static qml plugins.")
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${static_plugins_args_file}.copy" "${static_plugins_args_file}"
    )

    set_property(TARGET ${target} APPEND_STRING PROPERTY
        LINK_FLAGS " -Wl,-e,_qt_main_wrapper")
    set_property(TARGET ${target} APPEND_STRING PROPERTY
        LINK_FLAGS " @${static_plugins_args_file}")

    # So far Qt does not export dependent libraries for CMake, so we have to specify them manually.
    target_link_libraries(${target} PRIVATE
        "-framework MobileCoreServices"
        "-framework Foundation"
        "-framework UIKit"
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework MobileCoreServices"
        "-framework Foundation"
        "-framework UIKit"
        "-framework CoreFoundation"
        "-framework Security"
        "-framework SystemConfiguration"
        "-framework OpenGLES"
        "-framework QuartzCore"
        "-framework AssetsLibrary"
        "-L${QT_DIR}/lib"
        qtharfbuzzng qtpcre qtfreetype
        Qt5PlatformSupport
        Qt5LabsControls Qt5LabsTemplates
        z
    )

    set(plugins_import_cpp "${CMAKE_CURRENT_BINARY_DIR}/${target}_qt_plugins_import.cpp")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/build_utils/python/qmldeploy.py"
            --generate-import-cpp
            --qt-root ${QT_DIR} --qml-root "${APP_QML_ROOT}"
            --additional-plugins ${plugins}
            --output ${plugins_import_cpp}
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot generate plugins import file.")
    endif()

    target_sources(${target} PRIVATE ${plugins_import_cpp})

    set(app_dir "$<TARGET_FILE_DIR:${target}>")

    add_custom_command(TARGET ${target} PRE_LINK
        COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/build_utils/python/qmldeploy.py"
            --qt-root ${QT_DIR} --qml-root "${APP_QML_ROOT}"
            --output "${app_dir}/qt_qml"
        COMMENT "Copying QML imports for ${target}"
    )

    if(loginKeychainPassword)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND security unlock-keychain -p ${loginKeychainPassword} login.keychain)
    endif()
endfunction()
