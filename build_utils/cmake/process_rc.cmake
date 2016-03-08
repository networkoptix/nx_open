# Windows application icon
function(process_rc)
  if (WIN32)
    set(WINDOWS_RES_FILE ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.obj)
    if (MSVC)
      add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
        COMMAND rc.exe /fo ${WINDOWS_RES_FILE} ${PROJECT_BINARY_DIR}/module.rc
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
      )
      message ("Processing Resources from ${PROJECT_BINARY_DIR}/module.rc")
    else()
      add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
        COMMAND windres.exe ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME} ${WINDOWS_RES_FILE}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
      )
    endif()
  endif()
endfunction()