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

    //

    property QtObject label: QtObject
    {
        readonly property real height: 16;
        readonly property font font: Qt.font({ pixelSize: 12, weight: Font.Normal});
        readonly property color color: colors.windowText;
    }

    //

    property QtObject colors: QtObject
    {
        readonly property color window: context.getPaletteColor("dark", 3);
        readonly property color windowText: context.getPaletteColor("light", 1);

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
