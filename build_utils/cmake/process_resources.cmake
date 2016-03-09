function(process_resources)  
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/defines.h.cmake")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/defines.h.cmake
                   ${CMAKE_CURRENT_BINARY_DIR}/defines.h
                   )
  endif()
  file(GLOB_RECURSE FILTERED_RESOURCES "${PROJECT_SOURCE_DIR}/maven/filter-resources/*" "${CMAKE_SOURCE_DIR}/cpp/maven/filter-resources/*")
  foreach(filename ${FILTERED_RESOURCES})
    message("Filtering and Copying: " ${filename})  
    string(REPLACE "${PROJECT_SOURCE_DIR}/maven/filter-resources/" "" name ${filename})
    string(REPLACE "${CMAKE_SOURCE_DIR}/cpp/maven/filter-resources/" "" name ${name})    
    configure_file(${filename} ${CMAKE_CURRENT_BINARY_DIR}/${name})
  endforeach(filename ${FILTERED_RESOURCES})
  
  file(GLOB_RECURSE STATIC_RESOURCES "${PROJECT_SOURCE_DIR}/static-resources/*" "${PROJECT_SOURCE_DIR}/maven/bin-resources/resources/*" "${CMAKE_CURRENT_BINARY_DIR}/resources/*")
  file(WRITE ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc "<!DOCTYPE RCC>
")
  file(APPEND ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc "<RCC version=\"1.0\">
")
  file(APPEND ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc "<qresource prefix=\"/\">
")
  foreach(filename ${STATIC_RESOURCES})
    get_filename_component(STATIC_RESOURCE_DIR ${filename} DIRECTORY)
    get_filename_component(STATIC_RESOURCE_FILENAME ${filename} NAME)
    string(REPLACE "${PROJECT_SOURCE_DIR}/static-resources/" "" STATIC_RESOURCE_DIR ${STATIC_RESOURCE_DIR})
    string(REPLACE "${PROJECT_SOURCE_DIR}/maven/bin-resources/resources/" "" STATIC_RESOURCE_DIR ${STATIC_RESOURCE_DIR})
    string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/resources/" "" STATIC_RESOURCE_DIR ${STATIC_RESOURCE_DIR})
    file(APPEND ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc "  <file alias=\"${STATIC_RESOURCE_DIR}/${STATIC_RESOURCE_FILENAME}\">${filename}</file>
")
    message("Adding ${filename} to QRC")
  endforeach(filename ${STATIC_RESOURCES})
  file(APPEND ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc "</qresource>
")
  file(APPEND ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc "</RCC>
")
  #execute_process(COMMAND $ENV{environment}/python/x64/python.exe ${CMAKE_CURRENT_BINARY_DIR}/gen_resources.py)  
endfunction()