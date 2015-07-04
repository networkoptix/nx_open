import QtQuick 2.1;

import "../../common" as Common;

Rectangle
{
    id: thisComponent;

    height: Common.SizeManager.lineSizes.thin;

    color: "black";
    anchors
    {
        left: parent.left;
        right: parent.right;
    }
}
