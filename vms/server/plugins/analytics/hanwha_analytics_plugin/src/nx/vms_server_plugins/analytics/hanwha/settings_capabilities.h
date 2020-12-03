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
    struct Temperature
    {
        bool coordinate = false;
        bool temperatureType = false; //< Minimum/Maximum/Average
        bool detectionType = false; //< Above/Below/Increase/Decrease
        bool thresholdTemperature = false; //< F degree
        bool duration = false;
        bool areaEmissivity = false; // float [1..99]

        // parameters common for all temperature boxes:
        // bool btdEnable = false;
        // struct Overlay
        //{
        //    bool area;
        //    bool avgTemperature;
        //    bool minTemperature;
        //    bool maxTemperature;
        //};
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
    Temperature temperature;
    Mask mask;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
