## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    set(multivalue_args TRANSLATIONS ASSETS)
    cmake_parse_arguments(APP "" "" "${multivalue_args}" ${ARGN})

    qt6_import_qml_plugins(${target})

    set(app_dir "$<TARGET_FILE_DIR:${target}>")

    if(NOT developerBuild)
        add_custom_command(TARGET ${target} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${app_dir})
    endif()

    if (APP_ASSETS)
        add_custom_command(
            TARGET ${target}
            PRE_LINK
            DEPENDS ${APP_ASSETS}
            COMMAND ${CMAKE_COMMAND} -E copy ${APP_ASSETS} ${app_dir}
            COMMENT "Copying assets for ${target}"
        )
    endif()

    if(APP_TRANSLATIONS)
        set(translations_dir ${app_dir}/translations)
        add_custom_command(
            TARGET ${target}
            PRE_LINK
            DEPENDS ${APP_TRANSLATIONS}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${translations_dir}
            COMMAND ${CMAKE_COMMAND} -E copy ${APP_TRANSLATIONS} ${translations_dir}
            COMMENT "Copying translations for ${target}"
        )
    endif()

    # Copies Info.plist related translations to the bundle sources.
    set(source_plist_translations_dir ${CMAKE_CURRENT_LIST_DIR}/ios/translations)
    file(GLOB_RECURSE plist_source_translation_files "${source_plist_translations_dir}/*" )
    add_custom_command(
        TARGET ${target}
        PRE_LINK
        DEPENDS ${plist_source_translation_files}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${source_plist_translations_dir} ${app_dir}
        COMMENT "Copying plist translations for ${target}"
    )
endfunction()
