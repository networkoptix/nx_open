import QtQuick 2.4;
import QtQuick.Controls 1.1 as QtControls;
import QtQuick.Controls.Styles 1.1;

import "." as Base;
import "../../common" as Common;

QtControls.Button
{
    id: thisComponent;

    property bool thin: false;
    property color textColor: "#000000";

    property color normalColor: "#FFFFFF";
    property color hoveredColor: "#FFFFFF";
    property color activeColor: "#E9E9E9";
    property color disabledColor: "#80FFFFFF";

    property color normalBorderColor: "#D7D7D7";
    property color hoveredBorderColor: "#AEAEAE";
    property color activeBorderColor: "#AEAEAE";
    property color disabledBorderColor: normalBorderColor;

    property int fontSize: Common.SizeManager.fontSizes.medium;

    activeFocusOnPress: true;
    activeFocusOnTab: true;

    implicitWidth: metrics.width + Common.SizeManager.spacing.base * 3;
    implicitHeight: metrics.height + Common.SizeManager.spacing.base;

    TextMetrics
    {
        id: metrics;

        text: thisComponent.text;
        font.pixelSize: thisComponent.fontSize;
    }

    Keys.onReturnPressed: { clicked(); }
    Keys.onEnterPressed: { clicked(); }
    Keys.priority: Keys.BeforeItem;

    style: ButtonStyle
    {
        label: Base.Text
        {
            id: styleLabel;

            thin: thisComponent.thin;
            text: thisComponent.text;
            color: thisComponent.textColor;
            verticalAlignment: Text.AlignVCenter;
            horizontalAlignment: Text.AlignHCenter;
            font.pixelSize: thisComponent.fontSize;

            opacity: (thisComponent.enabled ? 1.0 : 0.5);
        }

        function getColor(forBackground)
        {
            if (!enabled)
                return (forBackground ? thisComponent.disabledColor : thisComponent.disabledBorderColor);
            else if (thisComponent.pressed)
                return (forBackground ? thisComponent.activeColor : thisComponent.activeBorderColor);
            else if (thisComponent.hovered || thisComponent.activeFocus)
                return (forBackground ? thisComponent.hoveredColor : thisComponent.hoveredBorderColor);
            else
                return (forBackground ? thisComponent.normalColor : thisComponent.normalBorderColor);
        }

        background: Rectangle
        {
            radius: 2;
            color: { getColor(true); }
            border.color: { getColor(false); }
        }
    }

    Rectangle
    {
        id: blinkArea;
        readonly property int offset: 3;

        x: -offset;
        y: -offset;
        width: parent.width + offset * 2;
        height: parent.height + offset * 2;

        opacity: 0;

        color: "#992FA2DB";
        radius: blinkArea.offset;

        Rectangle
        {
            anchors.fill: parent;

            color: "#00000000";

            border
            {
                width: 2;
                color: "#992FA2DB";
            }
            radius: blinkArea.offset;
        }

        SequentialAnimation
        {
            id: animtaion;
            loops: 2;

            NumberAnimation { target: blinkArea; property: "opacity"; to: 1; duration: 100; }
            NumberAnimation { target: blinkArea; property: "opacity"; to: 0; duration: 400; }
        }
    }

    function blink()
    {
        animtaion.start();
    }

}
