function(pick_file file_name result_name)
  if(file_name MATCHES "StdAfx.cpp")
    set(${result_name} FALSE PARENT_SCOPE)
    return()
  elseif(file_name MATCHES "_win[./]")
    if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      set(${result_name} FALSE PARENT_SCOPE)
      return()
    endif()
  elseif(file_name MATCHES "_unix[./]")
    if(NOT UNIX)
      set(${result_name} FALSE PARENT_SCOPE)
      return()
    endif()
  elseif(file_name MATCHES "_linux[./]")
    if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
      set(${result_name} FALSE PARENT_SCOPE)
      return()
    endif()
  elseif(file_name MATCHES "_mac[./]")
    if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin|iOS")
      set(${result_name} FALSE PARENT_SCOPE)
      return()
    endif()
  elseif(file_name MATCHES "_macx[./]")
    if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
      set(${result_name} FALSE PARENT_SCOPE)
      return()
    endif()
  elseif(file_name MATCHES "_android[./]")
    if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Android")
      set(${result_name} FALSE PARENT_SCOPE)
      return()
    endif()
  elseif(file_name MATCHES "_ios[./]")
    if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
      set(${result_name} FALSE PARENT_SCOPE)
      return()
    endif()
  endif()
  set(${result_name} TRUE PARENT_SCOPE)
endfunction()

function(find_sources src_dir var)
    file(GLOB_RECURSE CODE_FILES ${src_dir}/*.cpp)

    foreach(file_name ${CODE_FILES})
      pick_file(${file_name} PICK)
      if(${PICK})
        list(APPEND RESULT_FILES ${file_name})
      endif()
    endforeach()

    set(${var} ${RESULT_FILES} PARENT_SCOPE)
endfunction()
