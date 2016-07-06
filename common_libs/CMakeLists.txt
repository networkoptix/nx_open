project(common_libs)
set(PROJECT_SHORTNAME "common_libs")
set(PROJECT_LONGNAME "${short.company.name} Common Libraries")

add_definitions(-D_CONSOLE
                )

add_module(
    NAMES
        nx_audio
        nx_email
        nx_media
        nx_network
        nx_streaming
        nx_utils
        udt
    STATUS_SHORT_NAMES
)
