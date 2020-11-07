#pragma once

namespace nx::vms_server_plugins::analytics::hanwha {

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
    };

} // namespace nx::vms_server_plugins::analytics::hanwha
