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

    property string propertiesGroupName;

    property bool changed: false;

    anchors
    {
        left: parent.left;
        right: parent.right;
    }

    headerDelegate: FocusScope
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

            spacing: Common.SizeManager.spacing.small;

            FocusScope
            {
                id: header;

                height: Math.max(captionText.height, unsavedChangesMarker.height);
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                }

                Base.Text
                {
                    id: captionText;

                    anchors
                    {
                        verticalCenter: parent.verticalCenter;
                        left: parent.left;
                        right: unsavedChangesMarker.left;

                        leftMargin: Common.SizeManager.spacing.base;
                        rightMargin: Common.SizeManager.spacing.base;
                    }

                    thin: false;
                    font.pixelSize: Common.SizeManager.fontSizes.medium;
                    text: propertiesGroupName;
                }

                Rtu.UnsavedChangesMarker
                {
                    id: unsavedChangesMarker;

                    visible: thisComponent.changed;
                    anchors
                    {
                        verticalCenter: parent.verticalCenter;
                        right: parent.right;
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
            
            FocusScope
            {
                id: spacer;
                width: 1;
                height: Common.SizeManager.spacing.base;
            }
        }
    }
}

