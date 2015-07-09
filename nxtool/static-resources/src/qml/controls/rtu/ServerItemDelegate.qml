import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../rtu" as Rtu;
import "../base" as Base;
import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Item
{
    id: thisComponent;

    property int selectedState;
    property bool logged;
    property string serverName;
    property string macAddress;
    
    signal selectionStateShouldBeChanged(int currentItemIndex);
    signal explicitSelectionCalled(int currentItemIndex);
    
    height: column.height;

    opacity: (logged ? 1 : 0.3);
    
    Column
    {
        id: column;

        spacing: Common.SizeManager.spacing.base;
        anchors
        {
            left: parent.left;
            right: parent.right;
                
            leftMargin: Common.SizeManager.spacing.medium;
            rightMargin: Common.SizeManager.spacing.medium;
        }

        Item
        {
            id: bottomSpacer;
            
            width: parent.width;
            height: Common.SizeManager.spacing.small;
        }
        
        Item
        {
            id: placer;

            height: descriptionColumn.height;
            width: parent.width;

            Base.CheckBox
            {
                id: selectionCheckbox;
            
                anchors
                {
                    left: parent.left;
                    verticalCenter: parent.verticalCenter;                    
                }

                onClicked: 
                {
                    if (logged)
                        thisComponent.selectionStateShouldBeChanged(index); 
                }

                Binding
                {
                    target: selectionCheckbox;
                    property: "checkedState";
                    value: selectedState;
                }
            }
            
            Item
            {
                id: descriptionColumn;

                height: serverNameText.height + textSpacer.height
                    + (macAddressText.visible ? macAddressText.height : 0);
                
                anchors
                {
                    left: selectionCheckbox.right;
                    right: parent.right;
                    
                    leftMargin: Common.SizeManager.spacing.medium;
                    rightMargin: Common.SizeManager.spacing.medium;
                }

                Base.Text
                {
                    id: serverNameText;
                    
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }

                    text: serverName;
                    font.pixelSize: Common.SizeManager.fontSizes.base;
                }
                
                Item
                {
                    id: textSpacer;
                    
                    width: parent.width;
                    height: (visible ? Common.SizeManager.spacing.small : 0);
                    visible: macAddressText.visible;
                    
                    anchors.top: serverNameText.bottom;
                }

                Base.Text
                {
                    id: macAddressText;
                    
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                        top: textSpacer.bottom;
                    }
                    visible: (text.length !== 0);
                    text: macAddress;
                    font.pixelSize: Common.SizeManager.fontSizes.small;
                }
            }
        }

        Base.LineDelimiter
        {
            color: "#e4e4e4";
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
                thisComponent.explicitSelectionCalled(index);
        }

        Timer
        {
            id: clickFilterTimer;

            interval: 150;
            onTriggered: 
            {
                if (thisComponent.logged)
                    thisComponent.selectionStateShouldBeChanged(index); 
            }
        }
    }

    Rtu.Mark
    {
        enabled: logged;
        selected: (selectedState === Qt.Checked);
        anchors.fill: parent;
    }
}

