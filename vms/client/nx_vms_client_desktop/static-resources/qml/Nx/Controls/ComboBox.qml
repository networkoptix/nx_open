import QtQuick 2.11
import QtQuick.Controls 2.0
import Nx 1.0
import nx.client.desktop 1.0

import "private"

ComboBox
{
    id: control

    implicitWidth: 200
    implicitHeight: 28

    background: backgroundLoader.item

    Loader
    {
        id: backgroundLoader

        sourceComponent: control.editable ? textFieldBackground : buttonBackground

        Component
        {
            id: textFieldBackground

            TextFieldBackground { control: parent }
        }

        Component
        {
            id: buttonBackground

            ButtonBackground
            {
                hovered: control.hovered
                pressed: control.pressed

                FocusFrame
                {
                    anchors.fill: parent
                    anchors.margins: 1
                    visible: control.visualFocus
                    color: control.isAccentButton ? ColorTheme.brightText : ColorTheme.highlight
                    opacity: 0.5
                }
            }
        }
    }

    contentItem: TextInput
    {
        width: parent.width - indicator.width
        height: parent.height
        leftPadding: 8
        clip: true

        selectByMouse: true
        selectionColor: ColorTheme.highlight
        font.pixelSize: 14
        color: ColorTheme.text
        verticalAlignment: Text.AlignVCenter

        readOnly: !control.editable || control.down
        enabled: control.editable

        text: control.displayText

        opacity: control.enabled ? 1.0 : 0.3
    }

    indicator: Item
    {
        width: 28
        height: control.height

        anchors.right: control.right

        opacity: enabled ? 1.0 : 0.3

        MouseArea
        {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            cursorShape: Qt.ArrowCursor

            visible: control.editable
        }

        Rectangle
        {
            width: 26
            height: 26
            anchors.centerIn: parent
            color:
            {
                if (control.pressed)
                    return ColorTheme.lighter(ColorTheme.shadow, 1)

                if (hoverArea.containsMouse)
                    return ColorTheme.lighter(ColorTheme.shadow, 2)

                return "transparent"
            }

            visible: control.editable
        }

        ArrowIcon
        {
            anchors.centerIn: parent
            color: ColorTheme.text
            rotation: popup.opened ? 180 : 0
        }
    }

    delegate: ItemDelegate
    {
        height: 24
        width: control.width

        background: Rectangle
        {
            color: highlightedIndex == index ? ColorTheme.colors.brand_core : ColorTheme.midlight
        }

        contentItem: Text
        {
            anchors.fill: parent
            leftPadding: 8
            rightPadding: 8
            text: modelData
            elide: Text.ElideRight
            color: highlightedIndex == index ? ColorTheme.colors.brand_contrast : ColorTheme.text
            verticalAlignment: Text.AlignVCenter
        }
    }

    popup: Popup
    {
        y: control.height
        topMargin: 2
        bottomMargin: 2
        padding: 0
        width: control.width
        implicitHeight: contentItem.implicitHeight

        background: Rectangle
        {
            color: ColorTheme.midlight
            radius: 2
        }

        contentItem: ListView
        {
            clip: true
            model: control.popup.visible ? control.delegateModel : null
            implicitHeight: contentHeight
            currentIndex: control.highlightedIndex
        }
    }
}
