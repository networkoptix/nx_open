function(process_resources)
  file(GLOB_RECURSE FILTERED_FILES "${PROJECT_SOURCE_DIR}/resources/filter/*")
  foreach(filename ${FILTERED_FILES})
    string(REPLACE "${PROJECT_SOURCE_DIR}/resources/filter/" "" filename ${filename})
    configure_file(${PROJECT_SOURCE_DIR}/resources/filter/${filename}
                   ${CMAKE_CURRENT_BINARY_DIR}/${filename})
    message("Filtering and Copying: " ${filename})
  endforeach(filename ${FILTERED_FILES})

  file(GLOB_RECURSE COMMON_FILTERED_FILES "${CMAKE_SOURCE_DIR}/cpp/resources/filter/*")
  foreach(filename ${COMMON_FILTERED_FILES})
    string(REPLACE "${CMAKE_SOURCE_DIR}/cpp/resources/filter/" "" filename ${filename})
    configure_file(${CMAKE_SOURCE_DIR}/cpp/resources/filter/${filename}
                   ${CMAKE_CURRENT_BINARY_DIR}/${filename})
    message("Filtering and Copying: " ${filename})
  endforeach(filename ${COMMON_FILTERED_FILES})

  #configure_file(${CMAKE_SOURCE_DIR}/cpp/scripts/gen_resources.py
  #               ${CMAKE_CURRENT_BINARY_DIR}/gen_resources.py)
  #file(GLOB RESOURCES "${PROJECT_SOURCE_DIR}/resources/filter/*")
  #file(COPY ${RESOURCES} DESTINATION ${PROJECT_BINARY_DIR})
  execute_process(
      COMMAND $ENV{environment}/python/x64/python.exe ${CMAKE_CURRENT_BINARY_DIR}/gen_resources.py
  )  
endfunction()