import QtQuick 2.1;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;

Grid
{
    id: thisComponent;

    property alias model: repeater.model;

    verticalItemAlignment: Grid.AlignVCenter;
    columns: repeater.model.columnsCount;
    spacing: Common.SizeManager.spacing.medium;

    Component
    {
        id: requestStateComponent;

        Rectangle
        {
            property var value;

            width: Common.SizeManager.fontSizes.base;
            height: width;

            color: value ? "green" : "red";
            anchors.verticalCenter: parent.verticalCenter;
        }
    }

    Component
    {
        id: requestNameComponent;

        Base.Text
        {
            property var value;

            font
            {
                bold: true;
                pixelSize: Common.SizeManager.fontSizes.base;
            }
            text: value;
        }
    }

    Component
    {
        id: requestValueComponent;

        Base.Text
        {
            property var value;

            font.pixelSize: Common.SizeManager.fontSizes.base;
            text: value;
        }
    }

    Component
    {
        id: requestErrorReasonComponent;

        Base.Text
        {
            
            property var value;

            font.pixelSize: Common.SizeManager.fontSizes.base;
            color: "red";
            text: (value ? value : " ");
        }
    }
}
