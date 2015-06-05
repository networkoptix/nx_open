import QtQuick 2.4;

import "../controls/standard" as Rtu;
import "../controls" as Common;

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

        Rtu.Text
        {
            property var value;

            font
            {
                bold: true;
                pointSize: Common.SizeManager.fontSizes.base;
            }
            text: value;
        }
    }

    Component
    {
        id: requestValueComponent;

        Rtu.Text
        {
            property var value;

            font.pointSize: Common.SizeManager.fontSizes.base;
            text: value;
        }
    }

    Component
    {
        id: requestErrorReasonComponent;

        Rtu.Text
        {
            
            property var value;

            font.pointSize: Common.SizeManager.fontSizes.base;
            color: "red";
            text: (value ? value : " ");
        }
    }

    Repeater
    {
        id: repeater;

        delegate: Loader
        {
            id: loader;

            sourceComponent:
            {
                switch(dataType)
                {
                case 0:
                    return requestNameComponent;
                case 1:
                    return requestValueComponent;
                case 2:
                    return requestStateComponent;
                default:
                    return requestErrorReasonComponent
                }
            }

            Binding
            {
                property: "width";
                target: loader;
                value: item.width;
            }

            Binding
            {
                property: "height";
                target: loader;
                value: item.height;
            }

            Binding
            {
                target: item;
                property: "value";
                value: roleValue;
            }
        }
    }
}
