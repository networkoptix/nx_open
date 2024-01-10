// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core
import nx.vms.client.core
import nx.vms.client.desktop 1.0

SliderToolTip
{
    id: timeMarker

    property string dateText
    property string timeText

    property real minimumContentWidth: 128

    property int mode: { return Timeline.TimeMarkerMode.normal }
    property int display: { return Timeline.TimeMarkerDisplay.compact }

    roundingRadius: 2
    pointerLength: 3
    pointerWidth: 6

    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    shadowOffset: Qt.point(0, 0)
    shadowBlurRadius: 0

    contentItem: Item
    {
        id: content

        implicitWidth:
        {
            const desiredWidth = timeMarker.display !== Timeline.TimeMarkerDisplay.full
                ? (primaryLabel.implicitWidth + row.leftPadding + row.rightPadding)
                : row.implicitWidth

            return Math.max(timeMarker.minimumContentWidth, desiredWidth)
        }

        implicitHeight: 24

        Row
        {
            id: row

            anchors.horizontalCenter: content.horizontalCenter

            anchors.horizontalCenterOffset: secondaryLabel.automaticallyHidden
                ? Math.round((secondaryLabel.width + spacing) / 2)
                : 0

            leftPadding: 16
            rightPadding: 16
            topPadding: 3
            bottomPadding: 3
            spacing: 8

            Text
            {
                id: primaryLabel

                font.pixelSize: FontConfig.large.pixelSize
                font.bold: true
                color: timeMarker.textColor

                text: timeMarker.timeText ? timeMarker.timeText : timeMarker.dateText
                visible: !!text
            }

            Text
            {
                id: secondaryLabel

                font.pixelSize: FontConfig.large.pixelSize
                color: timeMarker.textColor

                text: timeMarker.timeText ? timeMarker.dateText : ""

                visible: text && timeMarker.display !== Timeline.TimeMarkerDisplay.compact

                readonly property bool automaticallyHidden:
                    timeMarker.display === Timeline.TimeMarkerDisplay.automatic
                        && timeMarker.contentItem.width < row.implicitWidth

                // Automatic hiding must be done via opacity because it should not affect
                // row implicit width.
                opacity: automaticallyHidden ? 0.0 : 1.0
            }
        }

        Image
        {
            id: leftArrow

            width: 5
            height: 10

            source: "image://svg/skin/timeline_tooltip/arrow_left.svg"
            visible: timeMarker.mode === Timeline.TimeMarkerMode.leftmost
            x: 8
            y: row.y + row.height / 2 - height / 2
        }

        Image
        {
            id: rightArrow

            width: 5
            height: 10

            source: "image://svg/skin/timeline_tooltip/arrow_right.svg"
            visible: timeMarker.mode === Timeline.TimeMarkerMode.rightmost
            x: content.width - width - 8
            y: row.y + row.height / 2 - height / 2
        }
    }

    // Assigned from c++ code with current timestamp for automated testing purposes.
    property real timestampMs: 0
}
