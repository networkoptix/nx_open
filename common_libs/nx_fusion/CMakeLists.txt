nx_add_target(nx_fusion LIBRARY
    PUBLIC_LIBS Qt5::Gui Qt5::Sql nx_utils)

target_include_directories(nx_fusion PRIVATE ${CMAKE_SOURCE_DIR}/common/src)
