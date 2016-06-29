import QtQuick 2.6;
import Qt.labs.controls 1.0;
import Qt.labs.templates 1.0 as T

import "."

// TODO: inner shadow
// TODO: implement exactly as in specification
// Note! Do not use currentText property - proper is just "text"
ComboBox
{
    id: thisComponent;

    property bool expanded: popup.visible;
    property bool editable: true;

    property bool isEditMode: false;
    property string text;

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

        onRowsRemoved:
        {
            if (currentIndex == -1)
                return;

            if (count === 0)
                currentIndex = -1;
            else if (currentIndex >= first && currentIndex <= last)
                currentIndex = -1;  // This item has been removed
            else if (currentIndex > last)
                currentIndex -= (last - first + 1);
        }

        onRowsInserted:
        {
            if (currentIndex == -1)
                currentIndex = 0;
            else if (currentIndex >= first)
            {
                var newCurrenIndex = currentIndex + (last - first + 1);
                if (newCurrenIndex < count)
                    currentItemIndex = newCurrenIndex;
                else
                    overflowCurrentIndex = newCurrenIndex;
            }
        }
        onRowsMoved:
        {
            if (currentIndex == -1)
                return;
            else if (currentIndex >= start && currentIndex <= end)
                currentIndex += (row - start);
            else if ((currentIndex < start) && (row <= currentIndex))
                currentIndex += (end - start + 1);
            else if ((currentIndex > end) && (row >= currentIndex))
                currentIndex -= (end - start + 1);
        }
    }

    textRole: "display";

    focus: true;

    height: 28;

    opacity: (enabled ? 1.0 : 0.3);


    onActiveFocusChanged:
    {
        if (activeFocus)
            textInputItem.forceActiveFocus();
    }

    Binding
    {
        target: thisComponent;
        property: "text";
        when: isEditMode;
        value: textInputItem.text;
    }

    MouseArea
    {
        id: modeChangeArea;

        height: parent.height;
        width: parent.width - 28;

        onClicked:
        {
            if (!thisComponent.editable)
            {
                mouse.accepted = false;
                return;
            }

            if (!thisComponent.isEditMode)
                thisComponent.isEditMode = true;

            textInputItem.forceActiveFocus();
        }
        visible: !thisComponent.isEditMode;
    }

    background: Rectangle
    {
        id: backgroundItem;

        anchors.fill: parent;

        color: (thisComponent.activeFocus
            ? Style.darkerColor(Style.colors.shadow, 1)
            : Style.colors.shadow);
        border.color: Style.darkerColor(Style.colors.shadow, 1);
        radius: 1;
    }

    contentItem: Item
    {
        anchors.fill: parent;

        TextInput
        {
            id: textInputItem;

            leftPadding: 8;
            rightPadding: 8;

            selectByMouse: true;
            clip: true;
            width: parent.width - indicatorItem.width;
            height: parent.height;
            visible: thisComponent.isEditMode;
            font: Style.textEdit.font;
            color: Style.textEdit.color;
            verticalAlignment: Text.AlignVCenter;

            KeyNavigation.tab: thisComponent.KeyNavigation.tab;
            KeyNavigation.backtab: thisComponent.KeyNavigation.backtab;

            onTextChanged:
            {
                thisComponent.currentIndex = thisComponent.find(text.trim()
                    , Qt.MatchExactly | Qt.MatchCaseSensitive);
            }
        }

        NxLabel
        {
            id: readOnlyTextItem;

            leftPadding: 8;
            rightPadding: 8;

            clip: true;
            anchors.fill: textInputItem;
            visible: !thisComponent.isEditMode;
            font: Style.textEdit.font;
            color: Style.textEdit.color;
            verticalAlignment: Text.AlignVCenter;
        }

        Rectangle
        {
            id: indicatorItem;
            width: 28;
            height: thisComponent.height;
            anchors.right: parent.right;
            color: backgroundItem.color;    // TODO: change

            Image
            {
                anchors.centerIn: parent;

                source: (thisComponent.expanded
                    ? "qrc:/skin/theme/drop_collapse.png"
                    : "qrc:/skin/theme/drop_expand.png");
            }
        }
    }


    popup: T.Popup
    {
        y: thisComponent.height;
        topMargin: 2;
        bottomMargin: 2;
        width: thisComponent.width;
        height: listViewItem.contentHeight;

        background: Rectangle
        {
            id: popupBackground;

            color: Style.dropDown.bkgColor;
            radius: 2;
            // TODO: shadow
        }

        contentItem: ListView
        {
            id: listViewItem;

            model: thisComponent.model;

            delegate: Rectangle
            {
                height: 24;
                width: parent.width;

                color: (popupItem.isHovered ? Style.dropDown.hoveredBkgColor
                    : Style.dropDown.bkgColor);

                NxLabel
                {
                    id: popupItem;
                    acceptClicks: true;
                    autoHoverable: true;
                    anchors.fill: parent;
                    clip: true;
                    leftPadding: 8;
                    rightPadding: 8;

                    standardColor: Style.dropDown.textColor;
                    hoveredColor: Style.dropDown.hoveredTextColor;

                    text: model[thisComponent.textRole];

                    onClicked:
                    {
                        thisComponent.currentIndex = index;
                        thisComponent.popup.visible = false;
                        thisComponent.updateText();
                    }
                }
            }
        }
    }

    onCurrentTextChanged:
    {
        if (isEditMode && currentIndex == -1)
            return;

        updateText();
    }

    function updateText()
    {
        textInputItem.text = thisComponent.currentText;
        readOnlyTextItem.text = thisComponent.currentText;
    }
}






















