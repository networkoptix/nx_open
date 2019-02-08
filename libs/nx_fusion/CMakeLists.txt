nx_add_target(nx_fusion LIBRARY
    PUBLIC_LIBS Qt5::Gui Qt5::Sql nx_utils)

if(withTests)
    add_subdirectory(unit_tests)
endif()
