import QtQuick 2.1

import "../base" as Base;
import "../../common" as Common;

Rectangle
{
    color: "antiquewhite";

    radius: height / 4;

    width: text.width + Common.SizeManager.spacing.base;
    height: text.height + Common.SizeManager.spacing.base;

    Base.Text
    {
        id: text;

        text: qsTr("unsaved changes");

        anchors.centerIn: parent;
    }
}

