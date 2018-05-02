import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    id: placeholder

    property alias text: label.text
    property alias secondaryText: secondaryLabel.text
    property alias description: descriptionLabel.text
    property alias icon: image.source
    property alias buttonText: button.text
    property alias secondaryButtonText: secondaryButton.text

    property color textColor: ColorTheme.transparent(ColorTheme.colors.red_core, 0.7)
    property color backgroundColor: ColorTheme.colors.dark5

    signal buttonClicked()
    signal secondaryButtonClicked()

    Rectangle
    {
        id: background
        anchors.fill: parent
        color: backgroundColor
    }

    AutoScalableItem
    {
        id: labelBlock

        anchors.horizontalCenter: parent.horizontalCenter

        transformOrigin: Item.Top

        minimumHeight: labelColumn.implicitHeight
        desiredHeight:
        {
            if (buttonsBlock.visible)
                return parent.height - buttonsBlock.actualHeight
            if (descriptionBlock.visible)
                return parent.height - descriptionBlock.actualHeight
            return parent.height
        }
        minimumWidth: 800
        desiredWidth: parent.width

        y: (desiredHeight - actualHeight) / 2

        Column
        {
            id: labelColumn

            width: parent.width
            anchors.centerIn: parent

            topPadding: label.lineCount > 1 ? 16 : 38
            bottomPadding: topPadding

            Image
            {
                id: image

                anchors.horizontalCenter: parent.horizontalCenter
                visible: status === Image.Ready
            }

            Text
            {
                id: label

                anchors.horizontalCenter: parent.horizontalCenter
                width: labelBlock.minimumWidth
                topPadding: 8
                bottomPadding: 8

                font.weight: Font.Thin
                font.pixelSize: 88
                color: textColor

                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight

                visible: text
            }

            Text
            {
                id: secondaryLabel

                anchors.horizontalCenter: parent.horizontalCenter
                width: labelBlock.minimumWidth

                font.pixelSize: 36
                color: textColor

                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight

                visible: text
            }
        }
    }

    AutoScalableItem
    {
        id: buttonsBlock

        visible: button.text || secondaryButton.text

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: buttonsColumn.implicitWidth

        desiredHeight: placeholder.height * 0.25
        minimumHeight: buttonsColumn.implicitHeight
        maximumHeight: (button.visible && secondaryButton.visible) ? 104 : 80

        transformOrigin: Item.Bottom

        Column
        {
            id: buttonsColumn

            spacing: 12
            topPadding: 16
            bottomPadding: 16
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width

            Button
            {
                id: button

                height: 32
                anchors.horizontalCenter: parent.horizontalCenter

                visible: text

                color: ColorTheme.transparent(ColorTheme.colors.light14, 0.15)
                hoveredColor: ColorTheme.transparent(ColorTheme.colors.light14, 0.2)
                textColor: ColorTheme.transparent(ColorTheme.colors.light14, 0.8)
                textHoveredColor: ColorTheme.colors.light14
                frameless: true

                onClicked: buttonClicked()
            }

            FlatButton
            {
                id: secondaryButton

                height: 16
                anchors.horizontalCenter: parent.horizontalCenter

                visible: text

                textColor: ColorTheme.transparent(ColorTheme.colors.light14, 0.5)
                textHoveredColor: ColorTheme.transparent(ColorTheme.colors.light14, 0.6)

                onClicked: secondaryButtonClicked()
            }
        }
    }

    AutoScalableItem
    {
        id: descriptionBlock

        visible: descriptionLabel.text && !buttonsBlock.visible

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        desiredHeight: placeholder.height * 0.25
        minimumHeight: 48
        maximumHeight: 80
        desiredWidth: parent.width - 32
        minimumWidth: 400
        maximumWidth: 600

        transformOrigin: Item.Bottom

        Text
        {
            id: descriptionLabel

            anchors.fill: parent

            color: ColorTheme.transparent(ColorTheme.colors.light14, 0.5)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignTop
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
        }
    }
}
