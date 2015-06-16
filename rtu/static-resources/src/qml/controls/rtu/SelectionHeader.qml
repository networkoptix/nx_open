import QtQuick 2.0;

import "../base" as Base;
import "../../common" as Common;

Item
{
    id: thisComponent;

    height: headerText.height + Common.SizeManager.spacing.base * 2;
    anchors
    {
        left: parent.left;
        right: parent.right;

        leftMargin: Common.SizeManager.spacing.base;
        rightMargin: Common.SizeManager.spacing.base;
    }

    Base.Text
    {
        id: headerText;

        anchors
        {
            left: parent.left;
            right: parent.right;
            verticalCenter: parent.verticalCenter;
        }

        color: "#666666";
        wrapMode: Text.Wrap;
        font.pixelSize: Common.SizeManager.fontSizes.large;
        horizontalAlignment: Text.AlignHCenter;

        text: qsTr("Auto-Detected Nx1 Devices and Systems.");
    }
}
