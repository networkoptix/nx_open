#pragma once

namespace nx::vms_server_plugins::analytics::hanwha {

struct TemperatureDetectionCapabilities
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

struct SettingsCapabilities
{
    //IvaLine
    bool ivaLineRuleName = false; //< `IvaLine::namedLineFigure::name`
    bool ivaLineObjectTypeFilter = false; //< `IvaLine::person` and `IvaLine::vehicle`

    // IvaArea
    bool ivaAreaRuleName = false; //< `IvaArea::namedPolygon::name`
    bool ivaAreaObjectTypeFilter = false; //< `IvaArea::person` and `IvaLine::vehicle`

    bool ivaAreaIntrusion = true;
    bool ivaAreaEnter = true;
    bool ivaAreaExit = true;
    bool ivaAreaAppear = false; //< appear/disappear is a single event
    bool ivaAreaLoitering = false;

    bool ivaAreaIntrusionDuration = true;
    bool ivaAreaAppearDuration = false;
    bool ivaAreaLoiteringDuration = false;

    TemperatureDetectionCapabilities temperatureDetection;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
