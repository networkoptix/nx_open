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

    property int overflowCurrentIndex: -1;

    onCountChanged:
    {
        if (overflowCurrentIndex === -1)
            return;

        // Workaround: below onRowsInserted may be connected to signal before combobox component, thus
        // count is not changed before it is processed. Thus we use this temp variable to store new
        // overflown index and update currentIndex in this case
        currentIndex = overflowCurrentIndex;
        overflowCurrentIndex = -1;
    }

    Connections
    {
        target: (model ? model : null);
        ignoreUnknownSignals: true;

        onDataChanged:
        {
            if ((topLeft.row > control.currentIndex)
                || (bottomRight.row < control.currentIndex))
            {
                return;
            }

            /**
              * Since dataChanged signal is not handled by ComboBox properly we have to reload
              * updated data manually. The best decision is to force index change because in this
              * case data for each role is updated
              */
            var lastIndex = control.currentIndex;
            control.currentIndex = -1;
            control.currentIndex = lastIndex;
        }

        onRowsRemoved:
        {
            if (currentIndex == -1)
                return;

            if (count === 0)
            {
                currentIndex = -1;
                return;
            }

            var removedCount = (last - first + 1);
            if (currentIndex >= first && currentIndex <= last)
            {
                var prevItemIndex = first - 1;
                if (prevItemIndex >= 0)
                {
                    currentIndex = prevItemIndex;
                }
                else
                {
                    var nextItemIndex = (last - removedCount) + 1;
                    if (nextItemIndex < count)
                        currentIndex = nextItemIndex;
                }
            }
            else if (currentIndex > last)
            {
                currentIndex -= removedCount;
            }
        }

        onRowsInserted:
        {
            if (currentIndex == -1)
                currentIndex = 0;
            else if (currentIndex >= first)
            {
                var newCurrenIndex = currentIndex + (last - first + 1);
                if (newCurrenIndex < count)
                    currentIndex = newCurrenIndex;
                else
                    overflowCurrentIndex = newCurrenIndex;
            }
        }

        property string lastText
        onModelAboutToBeReset: lastText = displayText
        onModelReset: updateTextTo(lastText)
    }

    onEditTextChanged: updateTextTo(editText)

    displayText: control.currentIndex === -1 && editable ? editText : currentText

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
        text: control.editText
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

    function updateTextTo(text)
    {
        var temp = text
        currentIndex = find(text.trim(), Qt.MatchExactly | Qt.MatchCaseSensitive)

        // Setting currentIndex to -1 will clear editText, so restoring it.
        if (currentIndex === -1)
            editText = temp
    }

}
