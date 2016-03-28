include(CMakeParseArguments)

function(add_module)
  # parse incoming arguments
  cmake_parse_arguments(
    _ADD_MODULE
    "STATUS_DISABLE;STATUS_SHORT_NAMES"
    "STATUS_RELATIVE_NAMES"
    "NAMES;CONDITION"
    ${ARGN}
  )
  # check arguments
  if(NOT _ADD_MODULE_NAMES)
    message(
      FATAL_ERROR
        "Module names list is undefined"
    )
  endif()
  if(_ADD_MODULE_STATUS_DISABLE)
    set(__add_module_status_enable 0)
  else()
    set(__add_module_status_enable 1)
  endif()
  # check and evaluate condition
  set(__exclude_module_from_build 1)
  if(NOT _ADD_MODULE_CONDITION)
    set(__exclude_module_from_build 0)
  elseif(${_ADD_MODULE_CONDITION})
    set(__exclude_module_from_build 0)
  endif()
  # include/exclude modules
  foreach(__module ${_ADD_MODULE_NAMES})
    if(${__add_module_status_enable})
      get_filename_component(
        __add_module_name
        ${__module}
        ABSOLUTE
      )
      if(_ADD_MODULE_STATUS_SHORT_NAMES)
        get_filename_component(
          __add_module_name
          ${__add_module_name}
          NAME
        )
      elseif(_ADD_MODULE_STATUS_RELATIVE_NAMES)
        file(
          RELATIVE_PATH
            __add_module_name
            ${_ADD_MODULE_STATUS_RELATIVE_NAMES}
            ${__add_module_name}
        )
      endif()
    endif()
    if(${__exclude_module_from_build})
      if(${__add_module_status_enable})
        message(STATUS "Exclude '${__add_module_name}' module from build")
      endif()
    else()
      if(${__add_module_status_enable})
        message(STATUS "Add module '${__add_module_name}'")
      endif()
      add_subdirectory(${__module})
    endif()
  endforeach()
endfunction(add_module)
