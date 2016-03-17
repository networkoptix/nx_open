SET(QT_CUSTOM_DIR "$ENV{environment}/qt5-custom")
function(add_qtsinglecoreapplication)
  get_filename_component(qtsinglecoreapplication.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtsinglecoreapplication.cpp ABSOLUTE)
  get_filename_component(qtlocalpeer.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtlocalpeer.cpp ABSOLUTE)
  SET(CPP_FILES 
      ${CPP_FILES} 
      ${qtlocalpeer.cpp}
      ${qtsinglecoreapplication.cpp}
     PARENT_SCOPE
     )
  include_directories (${include_directories} ${QT_CUSTOM_DIR}/qtsingleapplication/src $ENV{environment}/qt5-custom/qtsingleapplication/src)
endfunction()
function(add_qtsingleapplication)
get_filename_component(qtsingleapplication.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtsingleapplication.cpp ABSOLUTE)
get_filename_component(qtlocalpeer.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtlocalpeer.cpp ABSOLUTE)
  SET(CPP_FILES 
      ${CPP_FILES} 
      ${qtlocalpeer.cpp}
      ${qtsingleapplication.cpp}
      PARENT_SCOPE
     )
  include_directories (${include_directories} ${QT_CUSTOM_DIR}/qtsingleapplication/src $ENV{environment}/qt5-custom/qtsingleapplication/src)  
endfunction()
function(add_qtservice)
get_filename_component(qtservice.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtservice.cpp ABSOLUTE)
  SET(CPP_FILES 
      ${CPP_FILES} 
      ${qtservice.cpp}
     )
  if(WIN32)
     get_filename_component(qtservice_win.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtservice_win.cpp ABSOLUTE)  
     SET(CPP_FILES 
        ${CPP_FILES} 
qtservice
        PARENT_SCOPE
        )
  else()
     get_filename_component(qtservice_unix.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtservice_unix.cpp ABSOLUTE)
     get_filename_component(qtunixsocket.cpp ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtunixsocket.cpp ABSOLUTE)
     SET(CPP_FILES 
        ${CPP_FILES} 
        ${qtservice_unix}.cpp ${qtunixsocket}.cpp
        PARENT_SCOPE
        )
  endif()
  include_directories (${include_directories} ${QT_CUSTOM_DIR}/qtsingleapplication/src $ENV{environment}/qt5-custom/qtsingleapplication/src)  
endfunction()