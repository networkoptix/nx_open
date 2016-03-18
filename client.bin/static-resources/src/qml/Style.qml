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

    property QtObject custom: QtObject
    {
        property QtObject systemTile: QtObject
        {
            readonly property real systemNameLabelHeight: 24;
        }
    }

    //

    property QtObject colors: QtObject
    {
        readonly property color window: context.getPaletteColor("dark", 3);
        readonly property color windowText: context.getPaletteColor("light", 1);
        readonly property color shadow: context.getPaletteColor("dark", 4);
        readonly property color text: context.getPaletteColor("light", 13);
        readonly property color buttonText: text;
        readonly property color midlight: context.getPaletteColor("dark", 12);
        readonly property color button: context.getPaletteColor("dark", 10);

        readonly property color brand: context.getPaletteColor("brand", 4);
        readonly property color brandContrast: "white";//TODO: how to access brand_contrast color?

        readonly property color error: "red";

        property QtObject custom: QtObject
        {
            property QtObject systemTile: QtObject
            {
                readonly property color background: context.getPaletteColor("dark", 7);
                readonly property color systemNameText: context.getPaletteColor("light", 13);
            }
        }
    }

    property QtObject fonts: QtObject
    {
        property QtObject systemTile: QtObject
        {
            readonly property font systemName: Qt.font({ pixelSize: 20, weight: Font.Light});
            readonly property font info: Qt.font({ pixelSize: 12, weight: Font.Normal})
        }
    }
}
