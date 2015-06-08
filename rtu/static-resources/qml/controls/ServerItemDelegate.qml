import QtQuick 2.4;
import QtQuick.Controls 1.3;

import "../controls"

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

    Column
    {
        id: column;

        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        spacing: SizeManager.spacing.small;

        Item
        {
            id: topSpacer;
            height: SizeManager.spacing.base;

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
                
                width: SizeManager.clickableSizes.medium * 1.5;
                height: SizeManager.clickableSizes.medium;

                CheckBox
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

            Column
            {
                id: descriptionColumn;

                anchors
                {
                    left: selectionChackboxHolder.right;
                    right: loggedState.right;
                }

                spacing: SizeManager.spacing.small;

                Text
                {
                    id: caption;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }

                    font
                    {
                        bold: true;
                        pointSize: SizeManager.fontSizes.medium;
                    }
                    text: model.name;
                }

                Text
                {
                    id: macAddress;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }

                    font.pointSize: SizeManager.fontSizes.base;
                    text: model.macAddress;//qsTr("DB-SD-32-FD-41-HA-52");
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

        LineDelimiter
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

    SelectionMark
    {
        selected: (model.selectedState === Qt.Checked);
        anchors.fill: parent;
    }
}

