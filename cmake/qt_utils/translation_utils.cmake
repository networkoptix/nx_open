## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(_nx_add_generate_translations_target target)
    set(oneValueArgs
        SOURCE_DIRECTORY
        TARGET_FILE)
    cmake_parse_arguments(NX "" "${oneValueArgs}" "" ${ARGN})

    # Process all files from the /translations subdirectory. Ts files for each language must be
    # placed in the corresponding folder. They will be processed by lrelease, resulting qm files
    # will be placed to the qm build folder, then packed to a single external resources .dat file.
    if(NOT IS_DIRECTORY ${NX_SOURCE_DIRECTORY})
        message(FATAL_ERROR "Translations cannot be found in ${NX_SOURCE_DIRECTORY}")
    endif()

    set(qm_directory ${CMAKE_CURRENT_BINARY_DIR}/qm)
    set(needed_qm_files)

    # Processing only languages, allowed for the current customization.
    foreach(locale ${translations})
        set(locale_qm_directory ${qm_directory}/translations/${locale})
        file(MAKE_DIRECTORY "${locale_qm_directory}")
        file(GLOB ts_files CONFIGURE_DEPENDS "${NX_SOURCE_DIRECTORY}/${locale}/*.ts")
        set_source_files_properties(${ts_files} PROPERTIES OUTPUT_LOCATION "${locale_qm_directory}")
        qt5_add_translation(qm_files ${ts_files} OPTIONS -silent)
        foreach(qm_file ${qm_files})
            list(APPEND needed_qm_files ${qm_file})
        endforeach(qm_file)
    endforeach(locale)

    set_source_files_properties(${needed_qm_files}
        PROPERTIES RESOURCE_BASE_DIR "${qm_directory}")

    nx_add_external_resources_target(${target}
        TARGET_FILE ${NX_TARGET_FILE}
        QRC_FILE translations/${name}
        FILES ${needed_qm_files})

endfunction()

function(nx_make_target_translatable target)
    set(translations_target ${target}_translations)
    _nx_add_generate_translations_target(${translations_target}
        SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/translations
        TARGET_FILE ${translations_output_dir}/${target}.dat)
    add_dependencies(${target} ${translations_target})
endfunction()
