import QtQuick 2.1
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "." as Expandable;
import "../rtu" as Rtu;
import "../base" as Base;
import "../../common" as Common;

Expandable.ExpandableItem
{
    id: thisComponent;

    signal applyButtonPressed();
    
    property string propertiesGroupName;

    property bool changed: false;

    anchors
    {
        left: parent.left;
        right: parent.right;
    }

    headerDelegate: Item
    {
        id: headerHolder;

        height: propertiesHeader.height;

        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
        }

        Base.Column
        {
            id: propertiesHeader;

            anchors
            {
                left: parent.left;
                right: parent.right;
            }

            Item
            {
                id: header;

                height: headerRow.height;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                }

                Row
                {
                    id: headerRow;

                    spacing: Common.SizeManager.spacing.base;
                    anchors
                    {
                        left: parent.left;
                        top: parent.top;

                        leftMargin: Common.SizeManager.spacing.base;
                    }

                    Base.Text
                    {
                        id: captionText;

                        thin: false;
                        font.pixelSize: Common.SizeManager.fontSizes.medium;
                        text: propertiesGroupName;
                    }

                    Rtu.UnsavedChangesMarker
                    {
                        visible: thisComponent.changed;
                        anchors.verticalCenter: captionText.verticalCenter;
                    }
                }
            }

            Base.LineDelimiter
            {
                color: "#e4e4e4";
                
                anchors
                {
                   left: parent.left;
                   right: parent.right;
                }
            }
            
            Item
            {
                id: spacer;
                width: 1;
                height: Common.SizeManager.spacing.medium;
            }
        }
    }
}

