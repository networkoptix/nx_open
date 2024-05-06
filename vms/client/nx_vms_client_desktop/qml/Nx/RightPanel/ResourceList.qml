// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: resourceList

    property var resourceNames //< String list.
    property int maxDisplayedCount: 3
    readonly property int count: resourceNames ? resourceNames.length : 0
    readonly property int displayedCount: Math.min(count, maxDisplayedCount)
    readonly property bool empty: count === 0

    property color color: "grey"
    property color remainderColor: "grey"

    property real leftPadding: 0
    property real rightPadding: 0
    property real topPadding: 0
    property real bottomPadding: 0

    implicitWidth: layout.implicitWidth + leftPadding + rightPadding
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding

    ColumnLayout
    {
        id: layout

        spacing: 0

        anchors.fill: resourceList
        anchors.leftMargin: resourceList.leftPadding
        anchors.rightMargin: resourceList.rightPadding
        anchors.topMargin: resourceList.topPadding
        anchors.bottomMargin: resourceList.bottomPadding

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
                font { pixelSize: FontConfig.small.pixelSize; weight: Font.Medium }
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
            font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
        }
    }
}
