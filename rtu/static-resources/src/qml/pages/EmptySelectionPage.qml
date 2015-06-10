import QtQuick 2.1

import "../common" as Common;
import "../controls/base" as Base;

Item
{
    Base.Text
    {
        anchors.fill: parent;

        verticalAlignment: Text.AlignVCenter;
        horizontalAlignment: Text.AlignHCenter;
        wrapMode: Text.Wrap;

        font.pointSize: Common.SizeManager.fontSizes.large;
        text: qsTr("Please select servers");

        color: "grey";
    }
}

