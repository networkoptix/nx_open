set(GRAPHVIZ_EXTERNAL_LIBS "FALSE")
set(GRAPHVIZ_GENERATE_PER_TARGET "FALSE")
set(GRAPHVIZ_GENERATE_DEPENDERS "FALSE")
set(GRAPHVIZ_IGNORE_TARGETS
    ".*debug"
    "openssl"
    "sigar"
    "qt.*"
    "openal"
    "festival"
    "d3d9"
    "boost"
    "quazip"
    "ffmpeg"
    ".*_analytics_plugin"
    ".*_EXCLUDED"
    "generic.*plugin"
    "tegra_video"
    "mjpeg_link_plugin"
    "cassandra"
    "image_library_plugin"
)
