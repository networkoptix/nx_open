import QtQuick 2.0;

import "../base" as Base;
import "../../common" as Common;

Item
{
    id: thisComponent;
    property var parentColumn;
    property alias color: textItem.color;
    property alias text: textItem.text;
    property alias fontSize: textItem.font.pixelSize;

    height: Common.SizeManager.clickableSizes.medium;

    Base.Text
    {
        id: textItem;

        anchors.verticalCenter: parent.verticalCenter;

        font.pixelSize: Common.SizeManager.fontSizes.small;
        onWidthChanged:
        {
            var newVal = Common.SizeManager.spacing.large * 3 + textItem.width;
            if (parentColumn && (parentColumn.width < newVal))
                parentColumn.width = newVal;
        }

    }
}
