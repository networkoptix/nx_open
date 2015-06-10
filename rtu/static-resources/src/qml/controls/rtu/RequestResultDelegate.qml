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

    onColumnsChanged:
    {
        console.log("Columns: " + columns);
    }

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
                pointSize: Common.SizeManager.fontSizes.base;
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

            font.pointSize: Common.SizeManager.fontSizes.base;
            text: value;
        }
    }

    Component
    {
        id: requestErrorReasonComponent;

        Base.Text
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
