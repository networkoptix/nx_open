import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../rtu" as Rtu;
import "../base" as Base;
import "../../common" as Common;

Item
{
    id: thisComponent;

    property bool logged: model.logged;
    
    signal selectionStateShouldBeChanged(int currentItemIndex);
    signal explicitSelectionCalled(int currentItemIndex);

    height: column.height;
    anchors
    {
        left: parent.left;
        right: parent.right;
    }

    Base.Column
    {
        id: column;

        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        spacing: Common.SizeManager.spacing.small;

        Item
        {
            id: topSpacer;
            height: Common.SizeManager.spacing.base;

            anchors
            {
                left: parent.left;
                right: parent.right;
            }
        }

        Item
        {
            id: placer;

            height: descriptionColumn.height;
            anchors
            {
                left: parent.left;
                right: parent.right;
            }

            Rectangle
            {
                id: selectionChackboxHolder;
                
                width: Common.SizeManager.clickableSizes.medium * 1.5;
                height: Common.SizeManager.clickableSizes.medium;

                Base.CheckBox
                {
                    id: selectionCheckbox;

                    anchors.centerIn: parent;

                    onClicked: 
                    {
                        if (thisComponent.logged)
                        {
                            thisComponent.selectionStateShouldBeChanged(index); 
                        }
                        else
                        {
                            thisComponent.ss = 3;
                        }
                    }

                    Binding
                    {
                        target: selectionCheckbox;
                        property: "checkedState";
                        value: model.selectedState;
                    }
                }
            }

            Base.Column
            {
                id: descriptionColumn;

                anchors
                {
                    left: selectionChackboxHolder.right;
                    right: loggedState.right;
                }

                spacing: Common.SizeManager.spacing.small;

                Base.Text
                {
                    id: caption;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }

                    font.bold: true;
                    text: model.name;
                }

                Base.Text
                {
                    id: macAddress;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }

                    font.pointSize: Common.SizeManager.fontSizes.base;
                    text: model.macAddress;
                }
            }
            
            Rectangle
            {
                id: loggedState;
                
                height: width;
                width: selectionChackboxHolder.height;
                
                anchors
                {
                    verticalCenter: selectionChackboxHolder.verticalCenter;
                    right: parent.right;
                }

                color: (!thisComponent.logged ? "red" : (model.defaultPassword ? "white" : "green"));
            }

        }

        Base.LineDelimiter
        {
            color: "lightgrey";
            anchors
            {
                left: parent.left;
                right: parent.right;
            }
        }
    }

    MouseArea
    {
        id: explicitSelectionMouseArea;

        anchors.fill: parent;

        onClicked: { clickFilterTimer.start(); }
        onDoubleClicked:
        {
            clickFilterTimer.stop();
            
            if (thisComponent.logged)
            {
                thisComponent.explicitSelectionCalled(index);
            }
            else
            {
                thisComponent.ss = 3;
            }
        }

        Timer
        {
            id: clickFilterTimer;

            interval: 150;
            onTriggered: 
            {
                if (thisComponent.logged)
                {
                    thisComponent.selectionStateShouldBeChanged(index); 
                }
                else
                {
                    thisComponent.ss = 3;
                }
            }
        }
    }

    Rtu.SelectionMark
    {
        selected: (model.selectedState === Qt.Checked);
        anchors.fill: parent;
    }
}

