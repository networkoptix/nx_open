import QtQuick 2.1

import "../../common" as Common;
import "." as Base;

Base.Text
{
    id: thisComponent;

    property string hyperlink;
    property string caption: hyperlink;

//    linkColor: "#1DA6DC";

    font
    {
        pixelSize: Common.SizeManager.fontSizes.small;
        family: (thin ? Common.SizeManager.regularFont.name
            : Common.SizeManager.mediumFont.name );
    }

    text: qsTr("<a href=\"%1\">%2</a>").arg(hyperlink).arg(caption);
    onLinkActivated: { Qt.openUrlExternally(link); }

    MouseArea
    {
        anchors.fill: parent;
        acceptedButtons: Qt.NoButton; // pass all clicks to Text
        cursorShape: { parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }
}
