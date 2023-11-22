// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

import "date_utils.js" as DateUtils

Item
{
    height: 32

    property var range: NxGlobals.dateRange(
        new Date(1970, 1, 1), DateUtils.addDays(currentDate, 1))
    property date currentDate: new Date()

    signal intervalSelected(date startDate)

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
            color: ColorTheme.colors.dark4
            radius: parent.radius
        }

        Rectangle
        {
            width: parent.width
            height: parent.radius
            color: ColorTheme.colors.dark4
        }
    }

    Row
    {
        anchors.verticalCenter: parent.verticalCenter
        x: 8

        component IntervalButton: Item
        {
            property alias text: buttonText.text
            property real interval: 0

            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: buttonText.implicitWidth + 16
            implicitHeight: buttonText.implicitHeight

            enabled: getStartDate() <= range.end && getStartDate() >= range.start

            function getStartDate() { return new Date(currentDate.getTime() - interval) }

            Text
            {
                id: buttonText
                anchors.centerIn: parent
                font.pixelSize: FontConfig.small.pixelSize
                font.capitalization: Font.AllUppercase
                topPadding: 2
                bottomPadding: 2
                color: mouseArea.pressed
                    ? ColorTheme.colors.light6
                    : mouseArea.containsMouse
                        ? ColorTheme.colors.light1
                        : ColorTheme.buttonText
                opacity: enabled ? 1 : 0.3
            }

            MouseArea
            {
                id: mouseArea

                anchors.fill: parent
                hoverEnabled: true

                onPressed: intervalSelected(getStartDate())
            }
        }

        IntervalButton
        {
            text: qsTr("today")

            function getStartDate()
            {
                return DateUtils.stripTime(currentDate)
            }
        }
        IntervalButton
        {
            text: qsTr("-1 hour")
            interval: kMillisecondsPerHour
        }
        IntervalButton
        {
            text: qsTr("-24 hours")
            interval: 24 * kMillisecondsPerHour
        }
        IntervalButton
        {
            text: qsTr("-7 days")
            interval: 24 * 7 * kMillisecondsPerHour
        }
        IntervalButton
        {
            text: qsTr("-30 days")
            interval: 30 * 24 * 7 * kMillisecondsPerHour
        }
    }
}
