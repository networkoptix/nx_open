import QtQuick 2.6
import QtQuick.Layouts 1.11

ColumnLayout
{
    id: resourceList

    property var resourceNames //< String list.
    property int maxDisplayedCount: 3
    readonly property int count: resourceNames ? resourceNames.length : 0
    readonly property int displayedCount: Math.min(count, maxDisplayedCount)

    spacing: 0

    Repeater
    {
        model: displayedCount

        delegate: Text
        {
            id: resourceName

            Layout.fillWidth: true
            color: tile.palette.light
            elide: Text.ElideRight
            text: resourceNames[index]
            font { pixelSize: 11; weight: Font.Medium }
        }
    }

    Text
    {
        id: andMore
        readonly property int remainder: count - displayedCount

        color: tile.palette.windowText
        text: qsTr("...and %n more", "", remainder)
        visible: remainder > 0
        topPadding: 4
        font { pixelSize: 11; weight: Font.Normal }
    }
}
