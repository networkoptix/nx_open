import QtQuick 2.0;

import "." as Base;
import "../../common" as Common;

Base.Text
{
    property var parentOctet;

    text: ".";
    verticalAlignment: Qt.AlignBottom;
    font.pixelSize: Common.SizeManager.fontSizes.medium;

    MouseArea
    {
        anchors.fill: parent;
        cursorShape: Qt.IBeamCursor;

        onClicked:
        {
            if (parentOctet)
                parentOctet.forceActiveFocus();
        }
    }
}
