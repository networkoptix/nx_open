import QtQuick 2.4;

import "../controls";

Rectangle
{
    id: thisComponent;

    property bool thinDelimiter: true;

    height: (thinDelimiter ? SizeManager.lineSizes.thin : SizeManager.lineSizes.thick);
    radius: height / 2;

    color: "grey";
    opacity: (thinDelimiter ? 0.8 : 0.5);
    anchors
    {
        left: parent.left;
        right: parent.right;
        leftMargin: SizeManager.spacing.base;
        rightMargin: SizeManager.spacing.base;
    }
}
