set(video_core_dir "${PACKAGES_DIR}/rpi/video-core")
include(${video_core_dir}/video-core.cmake)

nx_add_server_plugin(rpi_cam LIBRARY
    NO_MOC PUBLIC_LIBS common nx_network nx_utils video_core)