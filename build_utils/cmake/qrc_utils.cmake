function(generate_qrc qrc_file)
    execute_process(
        COMMAND
            ${PYTHON_EXECUTABLE}
            ${PYTHON_MODULES_DIR}/gen_qrc.py
            ${ARGN}
            -o ${qrc_file}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "QRC generation script has failed.")
    endif()
endfunction()
