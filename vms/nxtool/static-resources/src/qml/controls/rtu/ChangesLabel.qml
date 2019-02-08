import QtQuick 2.1

import "../base" as Base;
import "../../common" as Common;

Rectangle
{
    property alias text: label.text

    radius: height / 4;

    width: label.width + Common.SizeManager.spacing.base;
    height: label.height + Common.SizeManager.spacing.base;

    Base.Text
    {
        id: label;
        anchors.centerIn: parent;
    }
}

