// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: resourceList

    property var resourceNames //< String list.
    property int maxDisplayedCount: 3
    readonly property int count: resourceNames ? resourceNames.length : 0
    readonly property int displayedCount: Math.min(count, maxDisplayedCount)

    property color color: "grey"
    property color remainderColor: "grey"

    spacing: 0

    Repeater
    {
        model: displayedCount

        delegate: Text
        {
            id: resourceName

            Layout.fillWidth: true
            color: resourceList.color
            elide: Text.ElideRight
            text: resourceNames[index]
            font { pixelSize: 11; weight: Font.Medium }
        }
    }

    Text
    {
        id: andMore
        readonly property int remainder: count - displayedCount

        color: resourceList.remainderColor
        text: qsTr("...and %n more", "", remainder)
        visible: remainder > 0
        topPadding: 4
        font { pixelSize: 11; weight: Font.Normal }
    }
}
