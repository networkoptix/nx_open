// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core.timeline

// Time marker, displaying position inside of the current timeline window.
Rectangle
{
    id: timeMarker

    property bool relevant: true //< Should be set instead of `visible`. Safe to bind to.
    property real positionMs: 0

    required property LabelFormatter labelFormatter
    required property TimeZone timeZone

    property real margin: 11
    property real lineSpacing: 5
    property real topRoundingRadius: 6
    property real bottomRoundingRadius: topRoundingRadius

    function hitTest(positionInParent)
    {
        return relevant && contains(mapFromItem(parent, positionInParent))
    }

    visible: relevant
    color: ColorTheme.colors.mobileTimeline.timeMarker.background

    width: 80
    height: labelFormatter.amPm ? 46 : 38

    topLeftRadius: LayoutMirroring.enabled ? 0 : topRoundingRadius
    topRightRadius: LayoutMirroring.enabled ? topRoundingRadius : 0
    bottomLeftRadius: LayoutMirroring.enabled ? 0 : bottomRoundingRadius
    bottomRightRadius: LayoutMirroring.enabled ? bottomRoundingRadius : 0

    Column
    {
        id: textLines

        anchors.left: timeMarker.left
        anchors.leftMargin: timeMarker.margin
        anchors.right: timeMarker.right
        anchors.rightMargin: timeMarker.margin
        anchors.verticalCenter: timeMarker.verticalCenter
        spacing: timeMarker.lineSpacing

        readonly property var lines:
        {
            if (!timeMarker.relevant) //< Don't format lines when not needed.
                return []

            return timeMarker.labelFormatter.timeMarkerLines(timeMarker.positionMs,
                timeMarker.timeZone)
        }

        TextLineTightWrapper
        {
            visible: !!amPmTextLine.text
            anchors.right: textLines.right

            textItem: Text
            {
                id: amPmTextLine

                text: textLines.lines[2] ?? ""
                color: ColorTheme.colors.mobileTimeline.timeMarker.tertiaryText
                font.pixelSize: 8
                font.weight: Font.DemiBold
            }
        }

        TextLineTightWrapper
        {
            visible: !!timeTextLine.text
            anchors.right: textLines.right

            textItem: Text
            {
                id: timeTextLine

                text: textLines.lines[1] ?? ""
                color: ColorTheme.colors.mobileTimeline.timeMarker.text
                font.pixelSize: 14
                font.weight: Font.Medium
            }
        }

        TextLineTightWrapper
        {
            visible: !!dateTextLine.text
            anchors.right: textLines.right

            textItem: Text
            {
                id: dateTextLine

                text: textLines.lines[0] ?? ""
                color: ColorTheme.colors.mobileTimeline.timeMarker.secondaryText
                font.pixelSize: 12
                font.weight: Font.Normal
                font.letterSpacing: timeMarker.labelFormatter.amPm ? -0.25 : 0
            }
        }
    }
}
