// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/debug.h>

#include "best_shots/plugin.h"
#include "custom_metadata/plugin.h"
#include "diagnostic_events/plugin.h"
#include "events/plugin.h"
#include "http_requests/plugin.h"
#include "motion_metadata/plugin.h"
#include "object_actions/plugin.h"
#include "object_detection/plugin.h"
#include "object_streamer/plugin.h"
#include "roi/plugin.h"
#include "sdk_features/plugin.h"
#include "settings/plugin.h"
#include "special_objects/plugin.h"
#include "taxonomy_features/plugin.h"
#include "video_frames/plugin.h"

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPluginByIndex(int instanceIndex)
{
    using namespace nx::vms_server_plugins::analytics::stub;

    switch (instanceIndex)
    {
        case 0: return new settings::Plugin();
        case 1: return new roi::Plugin();
        case 2: return new events::Plugin();
        case 3: return new diagnostic_events::Plugin();
        case 4: return new video_frames::Plugin();
        case 5: return new special_objects::Plugin();
        case 6: return new motion_metadata::Plugin();
        case 7: return new custom_metadata::Plugin();
        case 8: return new sdk_features::Plugin();
        case 9: return new taxonomy_features::Plugin();
        case 10: return new object_streamer::Plugin();
        case 11: return new best_shots::Plugin();
		case 12: return new object_detection::Plugin();
        case 13: return new object_actions::Plugin();
        case 14: return new http_requests::Plugin();
        default: return nullptr;
    }
}
