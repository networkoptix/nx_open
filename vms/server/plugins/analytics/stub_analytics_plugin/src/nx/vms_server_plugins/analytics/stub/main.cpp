// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/debug.h>

#include "best_shots_and_titles/integration.h"
#include "custom_metadata/integration.h"
#include "diagnostic_events/integration.h"
#include "events/integration.h"
#include "http_requests/integration.h"
#include "motion_metadata/integration.h"
#include "object_actions/integration.h"
#include "object_detection/integration.h"
#include "object_streamer/integration.h"
#include "roi/integration.h"
#include "sdk_features/integration.h"
#include "settings/integration.h"
#include "special_objects/integration.h"
#include "taxonomy_features/integration.h"
#include "video_frames/integration.h"
#include "error_reporting/integration.h"

extern "C" NX_PLUGIN_API nx::sdk::IIntegration* createNxPluginByIndex(int instanceIndex)
{
    using namespace nx::vms_server_plugins::analytics::stub;

    switch (instanceIndex)
    {
        case 0: return new settings::Integration();
        case 1: return new roi::Integration();
        case 2: return new events::Integration();
        case 3: return new diagnostic_events::Integration();
        case 4: return new video_frames::Integration();
        case 5: return new special_objects::Integration();
        case 6: return new motion_metadata::Integration();
        case 7: return new custom_metadata::Integration();
        case 8: return new sdk_features::Integration();
        case 9: return new taxonomy_features::Integration();
        case 10: return new object_streamer::Integration();
        case 11: return new best_shots::Integration();
		case 12: return new object_detection::Integration();
        case 13: return new object_actions::Integration();
        case 14: return new http_requests::Integration();
        case 15: return new error_reporting::Integration();
        default: return nullptr;
    }
}
