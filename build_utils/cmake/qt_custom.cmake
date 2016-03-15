SET(QT_CUSTOM_DIR "$ENV{environment}/qt5-custom")
function(add_qtsinglecoreapplication)
  SET(CPP_FILES 
     "${CPP_FILES} 
     ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtsinglecoreapplication.cpp
     ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtlocalpeer.cpp"
     PARENT_SCOPE
     )
  include_directories (${include_directories} ${QT_CUSTOM_DIR}/qtsingleapplication/src $ENV{environment}/qt5-custom/qtsingleapplication/src)
endfunction()
function(add_qtsingleapplication)
  SET(CPP_FILES 
     ${CPP_FILES} 
     ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtsingleapplication.cpp 
     ${QT_CUSTOM_DIR}/qtsingleapplication/src/qtlocalpeer.cpp
     )
  include_directories (${include_directories} ${QT_CUSTOM_DIR}/qtsingleapplication/src $ENV{environment}/qt5-custom/qtsingleapplication/src)  
endfunction()
function(add_qtservice)
  SET(CPP_FILES 
     ${CPP_FILES} 
     ${QT_CUSTOM_DIR}/qtservice/src/qtservice.cpp
     PARENT_SCOPE
     )
  if(WIN32)
     SET(CPP_FILES 
        ${CPP_FILES} 
        ${QT_CUSTOM_DIR}/qtservice/src/qtservice_win.cpp
        PARENT_SCOPE
        )
  else()
     SET(CPP_FILES 
        ${CPP_FILES} 
        ${QT_CUSTOM_DIR}/qtservice/src/qtservice_unix.cpp ${QT_CUSTOM_DIR}/qtservice/src/qtunixsocket.cpp
        PARENT_SCOPE
        )
  endif()
  include_directories (${include_directories} ${QT_CUSTOM_DIR}/qtsingleapplication/src $ENV{environment}/qt5-custom/qtsingleapplication/src)  
endfunction()