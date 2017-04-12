pragma Singleton

import QtQuick 2.5;

QtObject
{
    id: thisComponent;

    function getPaletteColor(color, offset)
    {
        return context.getPaletteColor(color, offset);
    }

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
        readonly property font font: Qt.font({ pixelSize: 13, weight: Font.Normal });
        readonly property color color: colors.windowText;
    }

    property QtObject menu: QtObject
    {
        readonly property real height: 24;
        readonly property font font: Qt.font({ pixelSize: 13, weight: Font.Normal });
        readonly property color color: colors.text;
        readonly property color colorHovered: colors.brandContrast;
        readonly property color background: colors.midlight;
        readonly property color backgroundHovered: colors.brand;
    }

    property QtObject textEdit: QtObject
    {
        readonly property font font: Qt.font({ pixelSize: 14, weight: Font.Normal });
        readonly property font fontReadOnly: Qt.font({ pixelSize: 14, weight: Font.Medium });
        readonly property color color: colors.text;
        readonly property color selectionColor: colors.highlight;
    }

    property QtObject dropDown: QtObject
    {
        readonly property color bkgColor: colors.midlight;
        readonly property color hoveredBkgColor: colors.brand;

        readonly property color textColor: colors.text;
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

        property QtObject banner: QtObject
        {
            property QtObject warning: QtObject
            {
                readonly property real backgroundOpacity: 0.2;
                readonly property color backgroundColor: colors.red_main;
                readonly property color textColor: "#CF8989";
            }
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
        readonly property color mid: context.getPaletteColor("dark", 9);
        readonly property color dark: context.getPaletteColor("dark", 8);
        readonly property color midlight: context.getPaletteColor("dark", 12);
        readonly property color button: context.getPaletteColor("dark", 10);
        readonly property color brand: context.getPaletteColor("brand", 7);

        readonly property color brandContrast: context.getContrastColor("brand");
        readonly property color highlight: brand;
        readonly property color red_main: context.getPaletteColor("red", 4);
        readonly property color yellow_main: context.getPaletteColor("yellow", 0);

        property QtObject custom: QtObject
        {
            property QtObject systemTile: QtObject
            {
                readonly property color disabled: colors.shadow;
                readonly property color background: context.getPaletteColor("dark", 7);
                readonly property color backgroundHovered: lighterColor(background);
                readonly property color factorySystemBkg: context.getPaletteColor("dark", 9);
                readonly property color factorySystemHovered: lighterColor(factorySystemBkg);

                readonly property color closeButtonBkg: context.getPaletteColor("dark", 9);
                readonly property color offlineIndicatorBkg: context.getPaletteColor("dark", 8);
                readonly property color setupSystemLink: context.getPaletteColor("light", 5);
            }

            property QtObject banner: QtObject
            {
                readonly property color link: context.getPaletteColor("blue", 2);
                readonly property color linkHovered: context.getPaletteColor("blue", 4);
            }

            property QtObject titleBar: QtObject
            {
                readonly property color shadow: colorWithAlpha(getPaletteColor("dark", 0), 0.15);
            }
        }
    }

    property QtObject fonts: QtObject
    {
        property font screenRecording: Qt.font({ pixelSize: 22, weight: Font.weight});

        property QtObject systemTile: QtObject
        {
            readonly property font systemName: Qt.font({ pixelSize: 20, weight: Font.Light});
            readonly property font info: Qt.font({ pixelSize: 12, weight: Font.Normal})
            readonly property font setupSystemLink: Qt.font({ pixelSize: 12, weight: Font.Medium});
            readonly property font indicator: Qt.font({ pixelSize: 10, weight: Font.Medium});
        }

        property QtObject banner: QtObject
        {
            readonly property font userName: Qt.font({ pixelSize: 22, weight: Font.Light});
        }

        readonly property font preloader: Qt.font({ pixelSize: 36, weight: Font.Light})

        property QtObject notFoundMessages: QtObject
        {
            readonly property font caption: Qt.font({ pixelSize: 24, weight: Font.Light});
            readonly property font description: Qt.font({ pixelSize: 13, weight: Font.Light});
        }
    }
}
