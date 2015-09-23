import QtQuick 2.1;

import "." as Base;
import "../../common" as Common;

Item
{
    id: thisComponent;

    property alias text: textControl.text;
    property int margin: Common.SizeManager.spacing.base;


    width: textControl.width + margin * 2;
    height: textControl.height + margin * 2;

    property Item globalParent: parent;

    property int oldX: x;
    property int oldY: y;

    opacity: 0;

    Behavior on opacity
    {
        SequentialAnimation
        {
            PropertyAnimation { duration: 50; }
        }
    }

    function show()
    {
        thisComponent.globalParent = thisComponent.parent;
        var p = thisComponent.parent;
        while (p.parent != undefined && p.parent.parent != undefined)
            p = p.parent;
        thisComponent.parent = p;

        thisComponent.oldX = thisComponent.x;
        thisComponent.oldY = thisComponent.y;
        var globalPos = mapFromItem(thisComponent.globalParent, thisComponent.oldX, thisComponent.oldY);

        thisComponent.x = globalPos.x + thisComponent.oldX;
        thisComponent.y = globalPos.y + thisComponent.oldY;

        thisComponent.opacity = 1;
    }

    function hide()
    {
        if (thisComponent.opacity == 0)
            return;

        thisComponent.opacity = 0;

        var oldClip = globalParent.clip;
        globalParent.clip = false;
        thisComponent.parent = globalParent;
        globalParent.clip = true;
        globalParent.clip = oldClip;
        thisComponent.x = thisComponent.oldX;
        thisComponent.y = thisComponent.oldY;
    }

    Rectangle
    {
        anchors.fill: parent;

        border.width: 1;
        smooth: true;
        radius: 4;

        color: "ivory";
    }

    Base.Text
    {
        id: textControl;

        x: thisComponent.margin;
        y: thisComponent.margin;
        font.pixelSize: Common.SizeManager.fontSizes.medium;
    }
}
