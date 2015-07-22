import QtQuick 2.0;
import QtQuick.Controls 1.1 as QtControls;
import QtQuick.Controls.Styles 1.1;

import "." as Base;
import "../../common" as Common;

QtControls.Button
{
    id: thisComponent;

    property color textColor: "white";
    property color buttonColor: "#35A0DA";
    property color pressedColor: "#00B1EC";
    property color borderColor: "#CCCCCC";

    property int fontSize: Common.SizeManager.fontSizes.medium;

    implicitHeight: Common.SizeManager.clickableSizes.base;

    activeFocusOnPress: true;

    style: ButtonStyle
    {
        label: Base.Text
        {
            id: styleLabel;

            anchors.fill: parent;

            text: thisComponent.text;
            wrapMode: Text.Wrap;
            color: thisComponent.textColor;
            verticalAlignment: Text.AlignVCenter;
            horizontalAlignment: Text.AlignHCenter;
            font.pixelSize: thisComponent.fontSize;
        }

        background: Rectangle
        {
            color: (enabled ? (thisComponent.pressed ? pressedColor : buttonColor) : borderColor);
            border.color: borderColor;
        }
    }
}
