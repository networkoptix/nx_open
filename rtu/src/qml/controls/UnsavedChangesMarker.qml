import QtQuick 2.0

import "../controls"

Rectangle
{
    color: "antiquewhite";

    radius: height / 4;

    width: text.width + SizeManager.spacing.base;
    height: text.height + SizeManager.spacing.base;

    Text
    {
        id: text;

        text: qsTr("unsaved changes");

        anchors.centerIn: parent;
    }
}

