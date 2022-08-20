// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Controls 1.0

Item
{
    height: 32

    signal intervalSelected(real milliseconds)

    readonly property real kMillisecondsPerHour: 60 * 60 * 1000

    Item
    {
        // Background rectangle which corners are rounded on the bottom and straight on the top.
        anchors.fill: parent

        layer.enabled: true

        readonly property real radius: 2

        Rectangle
        {
            anchors.fill: parent
            color: ColorTheme.colors.dark5
            radius: parent.radius
        }

        Rectangle
        {
            width: parent.width
            height: parent.radius
            color: ColorTheme.colors.dark5
        }
    }

    Row
    {
        anchors.verticalCenter: parent.verticalCenter
        x: 8

        Text
        {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Last")
            color: ColorTheme.midlight
            font.pixelSize: 11
            padding: 2
            rightPadding: 10
        }

        component IntervalButton: Rectangle
        {
            property alias text: buttonText.text

            signal pressed()

            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: buttonText.implicitWidth + 16
            implicitHeight: buttonText.implicitHeight

            color: "transparent"

            radius: 2
            border.width: 1
            border.color: mouseArea.containsMouse ? ColorTheme.colors.light16 : "transparent"

            Text
            {
                id: buttonText
                anchors.centerIn: parent
                color: ColorTheme.colors.light16
                font.pixelSize: 11
                topPadding: 2
                bottomPadding: 2
            }

            MouseArea
            {
                id: mouseArea

                anchors.fill: parent
                hoverEnabled: true

                onPressed: parent.pressed()
            }
        }

        IntervalButton
        {
            text: qsTr("Hour")
            onPressed: intervalSelected(kMillisecondsPerHour)
        }
        IntervalButton
        {
            text: qsTr("Day")
            onPressed: intervalSelected(24 * kMillisecondsPerHour)
        }
        IntervalButton
        {
            text: qsTr("Week")
            onPressed: intervalSelected(24 * 7 * kMillisecondsPerHour)
        }
        IntervalButton
        {
            text: qsTr("Month")
            onPressed:
            {
                const nowMs = Date.now().getTime()
                let then = new Date(nowMs)
                then.setMonth(then.getMonth() - 1)

                intervalSelected(nowMs - then.getTime())
            }
        }
    }
}
