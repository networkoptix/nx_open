// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stub_analytics_plugin_video_frames_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace video_frames {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace video_frames
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
