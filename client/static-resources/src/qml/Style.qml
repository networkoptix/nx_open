pragma Singleton

import QtQuick 2.5;
import NetworkOptix.Qml 1.0;

QtObject
{
    id: thisComponent;

    function darkerColor(color, offset)
    {
        if (!offset)
            offset = 1;
        return context.getDarkerColor(color, offset);
    }

    function lighterColor(color, offset)
    {
        if (!offset)
            offset = 1;
        return context.getLighterColor(color, offset);
    }

    function colorWithAlpha(color, alpha)
    {
        return context.colorWithAlpha(color, alpha);
    }

    //

    property QtObject label: QtObject
    {
        readonly property real height: 16;
        readonly property font font: Qt.font({ pixelSize: 13, weight: Font.Normal});
        readonly property color color: colors.windowText;
    }

    property QtObject textEdit: QtObject
    {
        readonly property font font: Qt.font({ pixelSize: 14, weight: Font.Normal });
        readonly property font fontReadOnly: Qt.font({ pixelSize: 14, weight: Font.Medium });
        readonly property color color: colors.text;
    }

    property QtObject dropDown: QtObject
    {
        readonly property color bkgColor: colors.text;
        readonly property color hoveredBkgColor: colors.brand;

        readonly property color textColor: context.getPaletteColor("dark", 3)
        readonly property color hoveredTextColor: colors.brandContrast;
    }

    property QtObject custom: QtObject
    {
        property QtObject systemTile: QtObject
        {
            readonly property real systemNameLabelHeight: 24;
            readonly property real shadowRadius: 24;
            readonly property int shadowSamples: 24;
        }
    }

    //

    property QtObject colors: QtObject
    {
        readonly property color window: context.getPaletteColor("dark", 3);
        readonly property color windowText: context.getPaletteColor("light", 1);
        readonly property color shadow: context.getPaletteColor("dark", 4);
        readonly property color text: context.getPaletteColor("light", 13);
        readonly property color light: context.getPaletteColor("light", 7);
        readonly property color buttonText: text;
        readonly property color brightText: context.getPaletteColor("light", 16);
        readonly property color midlight: context.getPaletteColor("dark", 12);
        readonly property color button: context.getPaletteColor("dark", 10);
        readonly property color brand: context.getPaletteColor("brand", 4);

        // TODO: #ynikitenkov add following block to json
        readonly property color brandContrast: "white";
        readonly property color red_main: "#CF2727";
        readonly property color yellow_main: "#FBBC05";

        property QtObject custom: QtObject
        {
            property QtObject systemTile: QtObject
            {
                readonly property color background: context.getPaletteColor("dark", 7);
                readonly property color setupSystem: context.getPaletteColor("light", 5);
                readonly property color factorySystemBkg: context.getPaletteColor("dark", 9);
                readonly property color factorySystemHovered: context.getPaletteColor("dark", 10);
                readonly property color closeButtonBkg: context.getPaletteColor("dark", 9);
                readonly property color offlineIndicatorBkg: context.getPaletteColor("dark", 8);
            }

            property QtObject banner: QtObject
            {
                readonly property color link: context.getPaletteColor("blue", 2);
                readonly property color linkHovered: context.getPaletteColor("blue", 4);
            }
        }
    }

    property QtObject fonts: QtObject
    {
        property QtObject systemTile: QtObject
        {
            readonly property font systemName: Qt.font({ pixelSize: 20, weight: Font.Light});
            readonly property font info: Qt.font({ pixelSize: 12, weight: Font.Normal})
            readonly property font setupSystem: Qt.font({ pixelSize: 12, weight: Font.Medium});
            readonly property font indicator: Qt.font({ pixelSize: 10, weight: Font.Medium});
        }

        property QtObject banner: QtObject
        {
            readonly property font userName: Qt.font({ pixelSize: 22, weight: Font.Light});
        }
    }
}
