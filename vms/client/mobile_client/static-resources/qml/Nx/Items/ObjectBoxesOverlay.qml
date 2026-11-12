// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core

Item
{
    id: overlay

    required property MediaPlayerAnalyticsProvider analyticsProvider
    required property MediaPlayer mediaPlayer

    property int index: 0 //< Video channel, set by MultiVideoOutput.

    /** Only objects intersecting this relative rectangle are shown. */
    property rect filterRect: Qt.rect(0, 0, 1, 1)

    /** If not null, only the object of this track is shown. */
    property var filterTrackId: NxGlobals.uuid("")

    property real lineWidth: 1
    property real radius: 2

    property real pointSize: 18
    property real pointLineWidth: 2

    AnalyticsObjectBoxesModel
    {
        id: objectBoxesModel

        metadataProvider: overlay.analyticsProvider.metadataProvider
        // The player may be released slightly before the overlay is destroyed.
        position: overlay.mediaPlayer?.displayedPosition ?? 0
        resource: overlay.mediaPlayer?.resource ?? null
        channel: overlay.index
        filterRect: overlay.filterRect
        filterTrackId: overlay.filterTrackId
    }

    // Metadata may be received after its video frame is displayed (e.g. while paused).
    Connections
    {
        target: overlay.analyticsProvider
        function onMetadataReceived() { objectBoxesModel.refresh() }
    }

    Repeater
    {
        id: objectBoxesView

        model: objectBoxesModel

        delegate: Item
        {
            id: object

            required property rect rect
            required property color color
            required property bool isPoint

            x: object.rect.x * overlay.width
            y: object.rect.y * overlay.height
            width: object.rect.width * overlay.width
            height: object.rect.height * overlay.height

            Rectangle
            {
                id: box

                anchors.fill: parent
                visible: !object.isPoint

                color: "transparent"
                radius: overlay.radius
                border.width: overlay.lineWidth
                border.color: object.color
            }

            Item
            {
                id: cross

                // Centered on the top left corner of the object rectangle.
                x: -width / 2
                y: -height / 2
                width: overlay.pointSize
                height: overlay.pointSize
                visible: object.isPoint

                Rectangle
                {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: overlay.pointLineWidth
                    color: object.color
                }

                Rectangle
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: overlay.pointLineWidth
                    height: parent.height
                    color: object.color
                }
            }
        }
    }
}
