import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

Control
{
    id: preview

    property string previewId
    property int previewState
    property real previewAspectRatio: 1

    padding: rectangle.border.width

    background: Rectangle
    {
        id: rectangle

        border.color: tile.palette.shadow
        color: tile.palette.dark
    }

    contentItem: Item
    {
        // Preview item should not have aspect ratio lesser than this value, by design.
        // Actual preview image is fit into these bounds keeping its own aspect ratio.
        readonly property real kItemMinimumAspectRatio: 16.0 / 9.0

        implicitHeight: width / Math.max(previewAspectRatio, kItemMinimumAspectRatio)

        Image
        {
            id: image

            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            visible: previewState == RightPanel.PreviewState.ready && previewId.length
            source: visible ? ("image://right_panel/" + previewId) : ""
        }

        Text
        {
            id: noData

            anchors.fill: parent
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
            visible: previewState == RightPanel.PreviewState.missing
            color: preview.palette.windowText
            text: qsTr("NO DATA")
        }

        NxDotPreloader
        {
            id: preloader

            anchors.centerIn: parent
            color: preview.palette.windowText

            visible: previewState == RightPanel.PreviewState.busy
                || previewState == RightPanel.PreviewState.initial
        }
    }
}
