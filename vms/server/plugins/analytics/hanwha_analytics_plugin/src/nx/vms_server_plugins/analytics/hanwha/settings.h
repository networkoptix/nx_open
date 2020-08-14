#pragma once

#include "setting_group.h"

namespace nx::vms_server_plugins::analytics::hanwha {

enum AnalyticsCategory
{
    motionDetection, //< currently not used
    shockDetection,
    tamperingDetection,
    defocusDetection,
    fogDetection,
    videoAnalytics, //< includes Passing, Intrusion, Entering, Exiting, Appearing, Loitering
    objectDetection, //< includes Person, Vehicle, Face, LicensePlate
    audioDetection,
    audioAnalytics, //< includes Scream, Gunshot, Explosion, GlassBreak
    faceMaskDetection,
    count //< number event of categories
};

using AnalyticsCategories = std::array<bool, AnalyticsCategory::count>;
//-------------------------------------------------------------------------------------------------

struct Settings
{
    static const int kMultiplicity = 4;
#if 0
    // Hanwha analyticsMode detection selection is currently removed from the Clients interface
    // desired analytics mode if selected implicitly now
    AnalyticsMode analyticsMode;
#endif
#if 0
    // Hanwha Motion detection support is currently removed from the Clients interface
    MotionDetectionObjectSize motionDetectionObjectSize;
    MotionDetectionIncludeArea motionDetectionIncludeArea[kMultiplicity];
    MotionDetectionExcludeArea motionDetectionExcludeArea[kMultiplicity];
#endif
    ShockDetection shockDetection;
    TamperingDetection tamperingDetection;
    DefocusDetection defocusDetection;
    FogDetection fogDetection;
    ObjectDetectionGeneral objectDetectionGeneral;
    ObjectDetectionBestShot objectDetectionBestShot;
    IvaObjectSize ivaObjectSize;
    IvaLine ivaLines[kMultiplicity];
    IvaArea ivaAreas[kMultiplicity];
    IvaExcludeArea ivaExcludeAreas[kMultiplicity];
    AudioDetection audioDetection;
    SoundClassification soundClassification;
    FaceMaskDetection faceMaskDetection;

    AnalyticsCategories analyticsCategories = {false};

    bool IntelligentVideoIsActive() const;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
