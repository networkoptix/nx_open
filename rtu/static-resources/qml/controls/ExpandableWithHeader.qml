import QtQuick 2.4;

import "../controls" as Common;
import "../controls/standard" as Rtu;

Common.ExpandableItem
{
    id: thisComponent;

    property string headerText;
    
    anchors
    {
        left: parent.left;
        right: parent.right;
    }

    headerDelegate: Column
    {
    
        spacing: Common.SizeManager.spacing.base;

        Item
        {
            height: headerTextControl.height;

            anchors
            {
                left: parent.left;
                right: parent.right;
                leftMargin: line.anchors.leftMargin;
            }

            Rtu.Text
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

        Common.LineDelimiter
        {
            id: line;
        }
    }
}

