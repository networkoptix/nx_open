import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "." as Base;
import "../../common" as Common;

ComboBox
{
    id: thisComponent;

    clip: true;

    dontUseWheel: true;

    activeFocusOnTab: pseudoEnabled;
    activeFocusOnPress: true;

    property bool pseudoEnabled: true;
    property int lastSelectedIndex;

    onHoveredChanged:
    {
        if (hovered)
            toolTip.show()
        else
            toolTip.hide();
    }

    onPressedChanged:
    {
        if (pressed)
            toolTip.hide();
    }

    signal timeZoneChanged(int from, int to);

    property bool changed: (currentIndex !== initIndex);

    property int initIndex: init;
    property int innerLastSelectedIndex;
    property int fontSize: Common.SizeManager.fontSizes.base;

    height: Common.SizeManager.clickableSizes.medium;
    width: height * 5;

    opacity: pseudoEnabled ? 1.0 : 0.5;

    currentIndex: initIndex;

    textRole: "display";

    property bool firstTimeChange: true;

    onCurrentIndexChanged:
    {
        if ((currentIndex == initIndex) && firstTimeChange)
        {
            firstTimeChange = false;
            innerLastSelectedIndex = currentIndex;
            lastSelectedIndex = currentIndex;
        }

        if (!model.isValidValue(currentIndex))
            return;

        if (currentIndex !== innerLastSelectedIndex)
            timeZoneChanged(innerLastSelectedIndex, currentIndex);

        lastSelectedIndex = innerLastSelectedIndex;
        innerLastSelectedIndex = currentIndex;
    }

    style: ComboBoxStyle
    {
        label: Base.Text
        {
            font.pixelSize: fontSize;
            text: thisComponent.currentText;
            verticalAlignment: Text.AlignVCenter;
            clip: true;

            anchors
            {
                left: parent.left;
                 right: parent.right;
                rightMargin: dropDownButtonWidth * 1.5;
            }
        }
    }

    Base.ToolTip
    {
        id: toolTip;

        globalParent: parent.parent;
        text: thisComponent.currentText;

        x: thisComponent.x;
        y: thisComponent.height;
    }

    MouseArea
    {
        visible: !pseudoEnabled;

        anchors.fill: parent;
        onEntered: { toolTip.show(); }
        onExited: { toolTip.hide(); }
    }
}









