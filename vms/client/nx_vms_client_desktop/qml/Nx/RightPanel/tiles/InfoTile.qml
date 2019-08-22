import QtQuick 2.6
import QtQuick.Controls.impl 2.4
import QtQuick.Layouts 1.11

import Nx.Controls 1.0

import "private"

TileBase
{
    id: tile

    readonly property bool isCloseable: (model && model.isCloseable) || false

    signal closeRequested()

    contentItem: RowLayout
    {
        id: contentLayout
        spacing: 8

        IconImage
        {
            id: icon

            width: 20
            height: 20
            sourceSize: Qt.size(width, height)
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop

            readonly property string decorationPath: (model && model.decorationPath) || ""
            source: decorationPath.length ? ("qrc:/skin/" + decorationPath) : ""
            color: caption.color
        }

        ColumnLayout
        {
            id: columnLayout
            spacing: 4

            Layout.topMargin: 2

            RowLayout
            {
                id: captionLayout
                spacing: 8

                Text
                {
                    id: caption

                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: (model && model.foregroundColor) || tile.palette.light
                    font { pixelSize: 13; weight: Font.Medium }

                    rightPadding: (isCloseable && !timestamp.text.length)
                        ? closeButton.width
                        : 0

                    text: (model && model.display) || ""
                }

                Text
                {
                    id: timestamp

                    Layout.minimumWidth: implicitWidth
                    Layout.alignment: Qt.AlignRight | Qt.AlignTop

                    topPadding: 2
                    color: tile.palette.windowText
                    visible: implicitWidth && implicitHeight && !closeButton.visible
                    font { pixelSize: 11; weight: Font.Normal }

                    text: (model && model.timestamp) || ""
                }
            }

            ResourceList
            {
                id: resourceList
                resourceNames: model && model.resourceList
            }

            Preview
            {
                id: preview

                Layout.fillWidth: true

                previewId: (model && model.previewId) || ""
                previewState: (model && model.previewState) || 0
                previewAspectRatio: (model && model.previewAspectRatio) || 1
                visible: (model && model.previewResource) || false
            }

            Text
            {
                id: description

                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: tile.palette.light
                font { pixelSize: 11; weight: Font.Normal }
                visible: implicitWidth && implicitHeight
                textFormat: Text.RichText

                text: (model && model.description) || ""
            }

            Text
            {
                id: footer

                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: tile.palette.light
                font { pixelSize: 11; weight: Font.Normal }
                visible: implicitWidth && implicitHeight
                textFormat: Text.RichText

                text: (model && model.additionalText) || ""
            }
        }
    }

    CloseButton
    {
        id: closeButton

        visible: isCloseable && tile.hovered

        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.top: parent.top
        anchors.topMargin: 6

        onClicked:
            tile.closeRequested()
    }
}
