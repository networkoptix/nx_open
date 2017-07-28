function(add_ios_ipa target)
    cmake_parse_arguments(IPA "" "TARGET;FILE_NAME" "" ${ARGN})

    set(payload_dir "${CMAKE_CURRENT_BINARY_DIR}/ipa/$<CONFIGURATION>/Payload/${IPA_TARGET}.app")
    set(app_dir "$<TARGET_FILE_DIR:${IPA_TARGET}>")

    add_custom_target(${target} ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${payload_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${payload_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${app_dir}" "${payload_dir}"
        COMMAND
            cd "$<CONFIGURATION>"
                && ${CMAKE_COMMAND} -E tar cfv "${IPA_FILE_NAME}" --format=zip "Payload"
        DEPENDS ${IPA_TARGET}
        BYPRODUCTS "${IPA_FILE_NAME}"
        VERBATIM
        COMMENT "Creating ${IPA_FILE_NAME}"
    )
endfunction()
