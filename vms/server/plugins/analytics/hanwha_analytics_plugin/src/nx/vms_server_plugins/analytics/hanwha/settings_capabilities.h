#pragma once

namespace nx::vms_server_plugins::analytics::hanwha {

struct SettingsCapabilities
{
    struct Tampering
    {
        bool enabled = false;
        bool thresholdLevel = false;
        bool sensitivityLevel = false;
        bool minimumDuration = false;
        bool exceptDarkImages = false;
    };
    struct Defocus
    {
        bool enabled = false;
        bool thresholdLevel = false;
        bool sensitivityLevel = false;
        bool minimumDuration = false;
    };
    struct TemperatureChange
    {
        // Since we have no access to a camera yet, let's set all parameters to true.
        bool unnamedRect = true;
        bool temperatureType = true; //< If true, supported types are Minimum/Maximum/Average
        bool temperatureGap = true; //< If true, supported gaps are 40/80/120/160/200
        bool duration = true;
    };
    struct BoxTemperature
    {
        bool unnamedRect = false;
        bool temperatureType = false; //< If true, supported types are Minimum/Maximum/Average
        bool detectionType = false; //< If true, supported types are Above/Below/Increase/Decrease
        bool thresholdTemperature = false; //< F degree
        bool duration = false;
        bool areaEmissivity = false; // float [1..99]
    };
    struct IvaLine
    {
        bool ruleName = false; //< `IvaLine::namedLineFigure::name`
        bool objectTypeFilter = false; //< `IvaLine::person` and `IvaLine::vehicle`
    };
    struct IvaArea
    {
        bool ruleName = false; //< `IvaArea::namedPolygon::name`
        bool objectTypeFilter = false; //< `IvaArea::person` and `IvaLine::vehicle`

        bool intrusion = true;
        bool enter = true;
        bool exit = true;
        bool appear = false; //< appear/disappear is a single event
        bool loitering = false;

        bool intrusionDuration = true;
        bool appearDuration = false;
        bool loiteringDuration = false;
    };
    struct Mask
    {
        bool enabled = false;
        bool detectionMode = false;
        bool duration = false;
    };

    Tampering tampering;
    Defocus defocus;
    bool videoAnalysis = false;
    bool videoAnalysis2 = false;
    IvaLine ivaLine;
    IvaArea ivaArea;
    TemperatureChange temperatureChange;
    BoxTemperature boxTemperature;
    Mask mask;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
