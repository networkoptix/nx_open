find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_executable(connection_mediator
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h)

add_precompiled_header(connection_mediator src/StdAfx.h FORCEINCLUDE)

target_include_directories(connection_mediator PUBLIC src)

target_link_libraries(connection_mediator libconnection_mediator)
