import QtQuick 2.1;

import "../base" as Base;
import "../expandable" as Expandable;
import "../../common" as Common;

Expandable.ExpandableItem
{
    id: thisComponent;

    property string headerText;
    
    anchors
    {
        left: parent.left;
        right: parent.right;
    }

    headerDelegate: Base.Column
    {
        Item
        {
            height: headerTextControl.height;

            anchors
            {
                left: parent.left;
                right: parent.right;
                leftMargin: line.anchors.leftMargin;
            }

            Base.Text
            {
                id: headerTextControl;
                text: thisComponent.headerText;
                font
                {
                    bold: true;
                    pointSize: Common.SizeManager.fontSizes.large;
                }
            }

            MouseArea
            {
                anchors.fill: parent;
                onClicked: { thisComponent.expanded = !thisComponent.expanded; }
            }
        }

        Base.LineDelimiter
        {
            id: line;
        }
    }
}

