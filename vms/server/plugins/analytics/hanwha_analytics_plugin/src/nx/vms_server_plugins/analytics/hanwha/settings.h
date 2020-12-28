#pragma once

#include "setting_group.h"

#include <vector>

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
    temperatureChangeDetection,
    boxTemperatureDetection,
    count //< number event of categories
};

using AnalyticsCategories = std::array<bool, AnalyticsCategory::count>;
//-------------------------------------------------------------------------------------------------

struct Settings
{
    static const int kMultiplicity = 4;
    static const int kTemperatureChangeMultiplicity = 3;
    static const int kBoxTemperatureMultiplicity = 3;
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
    std::vector<IvaLine> ivaLines;// [kMultiplicity] ;
    std::vector<IvaArea> ivaAreas;// [kMultiplicity] ;
    std::vector<IvaExcludeArea> ivaExcludeAreas;// [kMultiplicity] ;
    AudioDetection audioDetection;
    SoundClassification soundClassification;
    FaceMaskDetection faceMaskDetection;
    TemperatureChangeDetectionToggle temperatureChangeDetectionToggle;
    BoxTemperatureDetectionToggle boxTemperatureDetectionToggle;
    std::vector<TemperatureChangeDetection> temperatureChangeDetectionItems;
    std::vector<BoxTemperatureDetection> boxTemperatureDetectionItems;

    AnalyticsCategories analyticsCategories = {false};

    bool IntelligentVideoIsActive() const;

    Settings(const SettingsCapabilities& settingsCapabilities, const RoiResolution& roiResolution):
        shockDetection(settingsCapabilities, roiResolution),
        tamperingDetection(settingsCapabilities, roiResolution),
        defocusDetection(settingsCapabilities, roiResolution),
        fogDetection(settingsCapabilities, roiResolution),
        objectDetectionGeneral(settingsCapabilities, roiResolution),
        objectDetectionBestShot(settingsCapabilities, roiResolution),
        ivaObjectSize(settingsCapabilities, roiResolution),

        audioDetection(settingsCapabilities, roiResolution),
        soundClassification(settingsCapabilities, roiResolution),
        faceMaskDetection(settingsCapabilities, roiResolution),
        temperatureChangeDetectionToggle(settingsCapabilities, roiResolution),
        boxTemperatureDetectionToggle(settingsCapabilities, roiResolution)
    {
        for (int i = 0; i < kMultiplicity; ++i)
            ivaLines.emplace_back(settingsCapabilities, roiResolution, i);

        for (int i = 0; i < kMultiplicity; ++i)
            ivaAreas.emplace_back(settingsCapabilities, roiResolution, i);

        for (int i = 0; i < kMultiplicity; ++i)
            ivaExcludeAreas.emplace_back(settingsCapabilities, roiResolution, i);

        for (int i = 0; i < kTemperatureChangeMultiplicity; ++i)
            temperatureChangeDetectionItems.emplace_back(settingsCapabilities, roiResolution, i);
        for (int i = 0; i < kBoxTemperatureMultiplicity; ++i)
            boxTemperatureDetectionItems.emplace_back(settingsCapabilities, roiResolution, i);
    }
};

} // namespace nx::vms_server_plugins::analytics::hanwha
