import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

// TODO: inner shadow
// TODO: implement exactly as in specification

ComboBox
{
    id: control

    textRole: "display"

    implicitHeight: 28
    opacity: enabled ? 1.0 : 0.3

    displayText: (control.currentIndex === -1)
        ? control.editText
        : control.currentText

    background: Rectangle
    {
        color: control.activeFocus
            ? ColorTheme.darker(ColorTheme.shadow, 1)
            : ColorTheme.shadow
        border.color: ColorTheme.darker(ColorTheme.shadow, 1)
        radius: 2
    }

    contentItem: TextInput
    {
        anchors.fill: parent
        anchors.rightMargin: control.indicator.width

        leftPadding: 8
        rightPadding: 8

        selectByMouse: true
        selectionColor: ColorTheme.highlight
        clip: true
        width: parent.width - indicator.width
        height: parent.height
        font.pixelSize: 14
        color: ColorTheme.text
        verticalAlignment: Text.AlignVCenter

        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down

        KeyNavigation.tab: control.KeyNavigation.tab
        KeyNavigation.backtab: control.KeyNavigation.backtab

        onAccepted: updateControl()

        onActiveFocusChanged:
        {
            if (activeFocus)
            {
                text = control.editable ? control.editText : control.displayText
                selectAll()
            }
            else
            {
                updateControl()
            }
        }

        function updateControl()
        {
            var currentText = text

            control.currentIndex =
                control.find(text.trim(), Qt.MatchExactly | Qt.MatchCaseSensitive)

            if (control.currentIndex === -1)
            {
                // Setting currentIndex to -1 will clear editText, so restoring it.
                control.editText = currentText
            }
        }
    }

    indicator: Item
    {
        width: 28
        height: parent.height

        anchors.right: parent.right

        MouseArea
        {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            cursorShape: Qt.ArrowCursor
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
        }

        Image
        {
            anchors.centerIn: parent
            source: control.down
                ? "qrc:/skin/theme/drop_collapse.png"
                : "qrc:/skin/theme/drop_expand.png"
        }
    }

    delegate: ItemDelegate
    {
        height: 24
        width: control.width

        background: Rectangle
        {
            color: contentItem.isHovered ? ColorTheme.colors.brand_core : ColorTheme.midlight
            radius: 2
        }

        contentItem: NxLabel
        {
            autoHoverable: true
            anchors.fill: parent
            leftPadding: 8
            rightPadding: 8
            elide: Text.ElideRight

            standardColor: ColorTheme.text
            hoveredColor: ColorTheme.colors.brand_contrast

            text: index >= 0 ? model[control.textRole] : ""
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
            // TODO: shadow
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
