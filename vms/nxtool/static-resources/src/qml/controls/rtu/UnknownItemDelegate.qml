import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../base" as Base;
import "../../common" as Common;

Row
{
    id: thisComponent;

    property alias address: addressText.text;

    spacing: Common.SizeManager.spacing.medium;

    anchors
    {
        left: parent.left;
        right: parent.right;

        leftMargin: Common.SizeManager.spacing.medium;
        rightMargin: Common.SizeManager.spacing.medium;
    }

    Image
    {
        id: name

        width: addressText.height;
        height: addressText.height;

        source: "qrc:/resources/unknown.png"

    }

    Base.Text
    {
        id: addressText;
    }
}
