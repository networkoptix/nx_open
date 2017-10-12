pragma Singleton

import QtQuick 2.0
import Nx 1.0

QtObject
{
    id: thisComponent

    property QtObject label: QtObject
    {
        readonly property real height: 16
        readonly property font font: Qt.font({ pixelSize: 13, weight: Font.Normal })
    }

    property QtObject menu: QtObject
    {
        readonly property real height: 24
        readonly property font font: Qt.font({ pixelSize: 13, weight: Font.Normal })
    }

    property QtObject textEdit: QtObject
    {
        readonly property font font: Qt.font({ pixelSize: 14, weight: Font.Normal })
        readonly property font fontReadOnly: Qt.font({ pixelSize: 14, weight: Font.Medium })
    }

    property QtObject custom: QtObject
    {
        property QtObject systemTile: QtObject
        {
            readonly property real systemNameLabelHeight: 24
            readonly property real shadowRadius: 24
            readonly property int shadowSamples: 24
        }
    }

    //

    property QtObject fonts: QtObject
    {
        property font screenRecording: Qt.font({ pixelSize: 22, weight: Font.weight})

        property QtObject systemTile: QtObject
        {
            readonly property font systemName: Qt.font({ pixelSize: 20, weight: Font.Light})
            readonly property font info: Qt.font({ pixelSize: 12, weight: Font.Normal})
            readonly property font setupSystemLink: Qt.font({ pixelSize: 12, weight: Font.Medium})
            readonly property font indicator: Qt.font({ pixelSize: 10, weight: Font.Medium})
        }

        property QtObject banner: QtObject
        {
            readonly property font userName: Qt.font({ pixelSize: 22, weight: Font.Light})
        }

        readonly property font preloader: Qt.font({ pixelSize: 36, weight: Font.Light})

        property QtObject notFoundMessages: QtObject
        {
            readonly property font caption: Qt.font({ pixelSize: 24, weight: Font.Light})
            readonly property font description: Qt.font({ pixelSize: 13, weight: Font.Light})
        }
    }
}
