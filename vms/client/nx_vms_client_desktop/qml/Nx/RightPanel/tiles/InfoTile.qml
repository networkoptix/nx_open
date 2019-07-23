import QtQuick 2.6
import QtQuick.Controls.impl 2.4
import QtQuick.Layouts 1.11

TileBase
{
    id: tile

    property int maxDisplayedResourceCount: 3

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
            fillMode: Image.Pad
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop

            readonly property string decorationPath: (model && model.decorationPath) || ""
            source: decorationPath.length ? ("qrc:/skin/" + decorationPath) : ""
            color: (model && model.foregroundColor) || tile.palette.light
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
                    color: icon.color
                    font { pixelSize: 13; weight: Font.Medium }

                    rightPadding: (model.isCloseable && !timestamp.text.length)
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
                    visible: text.length && !tile.hovered
                    font { pixelSize: 11; weight: Font.Normal }

                    text: (model && model.timestamp) || ""
                }
            }

            ColumnLayout
            {
                id: resourceList

                spacing: 0
                readonly property var lines: model.resourceList
                readonly property int count: Math.min(lines.length, maxDisplayedResourceCount)
                readonly property int remainder: lines.length - count

                Repeater
                {
                    model: resourceList.count

                    delegate: Text
                    {
                        color: tile.palette.light
                        elide: Text.ElideRight
                        text: resourceList.lines[index]
                        font { pixelSize: 11; weight: Font.Medium }
                    }
                }

                Text
                {
                    id: andMore

                    color: tile.palette.windowText
                    text: qsTr("...and %n more", "", resourceList.remainder)
                    visible: resourceList.remainder > 0
                    topPadding: 4
                    font { pixelSize: 11; weight: Font.Normal }
                }
            }
        }
    }

    CloseButton
    {
        id: closeButton

        visible: tile.hovered && model.isCloseable

        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.top: parent.top
        anchors.topMargin: 6

        onClicked:
            tile.closeRequested()
    }
}
