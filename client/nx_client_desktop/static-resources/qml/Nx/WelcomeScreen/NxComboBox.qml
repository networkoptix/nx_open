import QtQuick 2.6;
import Qt.labs.controls 1.0;
import Qt.labs.templates 1.0 as T
import Nx 1.0;
import Nx.WelcomeScreen 1.0;

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

    signal accepted();

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
            if ((topLeft.row > thisComponent.currentIndex)
                || (bottomRight.row < thisComponent.currentIndex))
            {
                return;
            }

            /**
              * Since dataChanged signal is not handled by ComboBox properly we have to reload
              * updated data manually. The best decision is to force index change because in this
              * case data for each role is updated
              */
            var lastIndex = thisComponent.currentIndex;
            thisComponent.currentIndex = -1;
            thisComponent.currentIndex = lastIndex;
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
    }

    textRole: "display";

    focus: true;

    height: 28;
    opacity: (enabled ? 1.0 : 0.3);

    onActiveFocusChanged:
    {
        if (activeFocus)
            textInputItem.forceActiveFocus();
        else if (popup)
            popup.visible = false;
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
        hoverEnabled: true;

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

        color: thisComponent.activeFocus
            ? ColorTheme.darker(ColorTheme.shadow, 1)
            : ColorTheme.shadow;
        border.color: ColorTheme.darker(ColorTheme.shadow, 1);
        radius: 2;
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
            selectionColor: ColorTheme.highlight;
            clip: true;
            width: parent.width - indicatorItem.width;
            height: parent.height;
            visible: thisComponent.isEditMode;
            font: Style.textEdit.font;
            color: ColorTheme.text;
            verticalAlignment: Text.AlignVCenter;

            KeyNavigation.tab: thisComponent.KeyNavigation.tab;
            KeyNavigation.backtab: thisComponent.KeyNavigation.backtab;
            onAccepted: thisComponent.accepted();
            onTextChanged:
            {
                thisComponent.currentIndex = thisComponent.find(text.trim()
                    , Qt.MatchExactly | Qt.MatchCaseSensitive);
            }
            onActiveFocusChanged:
            {
                if (activeFocus)
                    selectAll();
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
            color: ColorTheme.text;
            verticalAlignment: Text.AlignVCenter;
        }

        Item
        {
            id: indicatorItem;
            width: 28;
            height: thisComponent.height;
            anchors.right: parent.right;
            visible: thisComponent.isEditMode;
            Rectangle
            {
                width: 26;
                height: 26;
                anchors.centerIn: parent;
                color:
                {
                    if (thisComponent.pressed)
                        return ColorTheme.lighter(ColorTheme.shadow, 1);
                    if (hoverArea.containsMouse)
                        return ColorTheme.lighter(ColorTheme.shadow, 2);
                    return backgroundItem.color;
                }
            }

            Image
            {
                anchors.centerIn: parent;

                source: (thisComponent.expanded
                    ? "qrc:/skin/theme/drop_collapse.png"
                    : "qrc:/skin/theme/drop_expand.png");
            }

            MouseArea
            {
                id: hoverArea;

                anchors.fill: parent;
                hoverEnabled: true;
                acceptedButtons: Qt.NoButton;
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

            color: ColorTheme.midlight;
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
                radius: 2;

                color: popupItem.isHovered ? ColorTheme.colors.brand_core : ColorTheme.midlight;

                NxLabel
                {
                    id: popupItem;
                    acceptClicks: true;
                    autoHoverable: true;
                    anchors.fill: parent;
                    clip: true;
                    leftPadding: 8;
                    rightPadding: 8;
                    elide: Text.ElideRight;

                    standardColor: ColorTheme.text;
                    hoveredColor: ColorTheme.colors.brand_contrast;

                    text: index >= 0 ? model[thisComponent.textRole] : "";

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
