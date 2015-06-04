import QtQuick 2.3
import QtQuick.Controls 1.3;
import QtQuick.Controls.Styles 1.3;


import ".";
import "standard" as Rtu;

ExpandableItem
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

        Column
        {
            id: propertiesHeader;

            spacing: SizeManager.spacing.base;
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

                    spacing: SizeManager.spacing.base;
                    anchors
                    {
                        left: parent.left;
                        top: parent.top;

                        leftMargin: SizeManager.spacing.base;
                    }

                    Rtu.Text
                    {
                        id: captionText;

                        font
                        {
                            bold: true;
                            pointSize: SizeManager.fontSizes.large;
                        }

                        text: propertiesGroupName;
                    }

                    UnsavedChangesMarker
                    {
                        visible: thisComponent.changed;
                        anchors.verticalCenter: captionText.verticalCenter;
                    }

                }

                Rtu.Button
                {
                    id: toggleButton;

                    width: height;
                    anchors
                    {
                        right: parent.right;
                        verticalCenter: header.verticalCenter;
                    }

                    text: (thisComponent.expanded ? "^" : "v");
                    onClicked: { thisComponent.toggle(); }
                }

            }

            LineDelimiter
            {
                id: line;
                anchors
                {
                   left: parent.left;
                   right: parent.right;
                }
            }
        }

        MouseArea
        {
            id: expandMouseArea;

            anchors.fill: parent;

            onClicked: { thisComponent.toggle(); }
        }
    }
}

