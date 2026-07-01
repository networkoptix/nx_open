// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.core.timeline

// Header or footer time marker, displaying position outside of the current timeline window.
Rectangle
{
    id: externalTimeMarker

    property bool relevant: true //< Should be set instead of `visible`. Safe to bind to.
    property real positionMs: -1

    required property LabelFormatter labelFormatter
    required property TimeZone timeZone

    required property real widthAtLive

    property bool interactive: true
    property real margin: 12
    property real topRoundingRadius: 6

    signal tapped()

    visible: relevant
    color: ColorTheme.colors.mobileTimeline.timeMarker.background

    width: positionMs < 0
        ? widthAtLive
        : (textItem.implicitWidth + margin * 2)

    height: 23

    topLeftRadius: LayoutMirroring.enabled ? 0 : topRoundingRadius
    topRightRadius: LayoutMirroring.enabled ? topRoundingRadius : 0

    Text
    {
        id: textItem

        anchors.right: externalTimeMarker.right
        anchors.rightMargin: externalTimeMarker.margin
        anchors.verticalCenter: externalTimeMarker.verticalCenter

        width: Math.min(implicitWidth, externalTimeMarker.width - externalTimeMarker.margin * 2)

        text:
        {
            if (!externalTimeMarker.relevant)
                return ""

            if (externalTimeMarker.positionMs < 0)
                return "Live"

            return externalTimeMarker.labelFormatter.externalTimeMarker(
                externalTimeMarker.positionMs, externalTimeMarker.timeZone)
        }

        font.pixelSize: 12
        color: ColorTheme.colors.mobileTimeline.timeMarker.text
        verticalAlignment: Qt.AlignVCenter
    }

    ColoredImage
    {
        id: liveIcon

        visible: externalTimeMarker.positionMs < 0

        anchors.right: textItem.left
        anchors.rightMargin: 2
        anchors.verticalCenter: externalTimeMarker.verticalCenter

        sourcePath: "image://skin/16x16/Solid/play_arrow.svg"
        sourceSize: Qt.size(16, 16)
        primaryColor: textItem.color
    }

    TapHandler
    {
        acceptedButtons: Qt.LeftButton
        enabled: externalTimeMarker.interactive

        onTapped:
            externalTimeMarker.tapped()
    }
}
