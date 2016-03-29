import QtQuick 2.6;
import Qt.labs.controls 1.0;
import Qt.labs.templates 1.0 as T

import "."

// TODO: inner shadow

ComboBox
{
    id: thisComponent;

    property bool expanded: popupItem.visible;
    property bool editable: true;

    property bool isEditMode: false;

    focus: true;

    height: 28;

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
    }

    background: Rectangle
    {
        id: backgroundItem;

        anchors.fill: parent;

        color: Style.colors.shadow;
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

            clip: true;
            width: parent.width - indicatorItem.width;
            height: parent.height;
            visible: thisComponent.isEditMode;
            font: Style.textEdit.font;
            color: Style.textEdit.color;
            verticalAlignment: Text.AlignVCenter;
        }

        NxLabel
        {
            clip: true;
            anchors.fill: textInputItem;
            visible: !thisComponent.isEditMode;
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
        id: popupItem;

        contentItem: Item
        {
            onVisibleChanged: console.log("visible:", visible)
        }
    }
}
