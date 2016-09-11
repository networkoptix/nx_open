# project(common_libs)
# set(PROJECT_SHORTNAME "common_libs")
# set(PROJECT_LONGNAME "${short.company.name} Common Libraries")
#
# add_definitions(-D_CONSOLE
#                 )
#
# add_module(
#     NAMES
#         nx_audio
#         nx_email
#         nx_media
#         nx_network
#         nx_streaming
#         nx_utils
#         udt
#     STATUS_SHORT_NAMES
# )

add_subdirectory(nx_utils)
add_subdirectory(nx_fusion)
# add_subdirectory(nx_streaming)
# add_subdirectory(nx_network)
add_subdirectory(udt)
