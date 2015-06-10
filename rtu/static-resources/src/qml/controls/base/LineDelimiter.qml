import QtQuick 2.1;

import "../../common" as Common;

Rectangle
{
    id: thisComponent;

    property bool thinDelimiter: true;

    height: (thinDelimiter ? Common.SizeManager.lineSizes.thin
        : Common.SizeManager.lineSizes.thick);
    radius: height / 2;

    color: "grey";
    opacity: (thinDelimiter ? 0.8 : 0.5);
    anchors
    {
        left: parent.left;
        right: parent.right;
        leftMargin: Common.SizeManager.spacing.base;
        rightMargin: Common.SizeManager.spacing.base;
    }
}
