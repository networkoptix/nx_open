import QtQuick 2.4;
import QtQuick.Controls 1.3;

import "../controls"
import "standard" as Rtu;
import "../dialogs" as Dialogs;

Item
{
    id: thisComponent;

    signal selectionStateShouldBeChanged(int currentItemIndex);

    height: topColumn.height;

    anchors
    {
        left: parent.left;
        right: parent.right;
    }

    Column
    {
        id: topColumn;
        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        Item
        {
            height: column.height;
            anchors
            {
                left: parent.left;
                right: parent.right;
            }
            
            Column
            {
                id: column;
        
                spacing: SizeManager.spacing.base;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                }
        
                Item
                {
                    id: spacer;
        
                    height: SizeManager.spacing.base;
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }
                }
        
                Item
                {
                    id: descriptionRow;
        
                    height: systemName.height;
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }
        
                    Item
                    {
                        id: selectionCheckboxWrapper;
        
                        width: SizeManager.clickableSizes.medium;
                        height: width;
                    
                        anchors.verticalCenter: parent.verticalCenter;
        
                        CheckBox
                        {
                            id: selectionCheckbox;
        
                            anchors.centerIn: parent;
                            onClicked: { thisComponent.selectionStateShouldBeChanged(index); }
        
                            Binding
                            {
                                target: selectionCheckbox;
                                property: "checkedState";
                                value: model.selectedState;
                            }
                        }
                        
                    }
        
                    Text
                    {
                        id: systemName;
        
                        height: SizeManager.clickableSizes.base;
        
                        anchors
                        {
                            left: selectionCheckboxWrapper.right;
                            right: parent.right;
                            verticalCenter: parent.verticalCenter;
        
                        }
        
                        font.pointSize: SizeManager.fontSizes.large;
                        text: (!model.name || (model.name.length === 0) ? qsTr("NO SYSTEM") : model.name);
                    }
                }
        
                LineDelimiter
                {
                    thinDelimiter: false;
                    color: "grey";
        
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }
                }
            }
            
            MouseArea
            {
                id: selectionMouseArea;
        
                height: systemName.height;
                anchors.fill: column;
        
        
                onClicked: { thisComponent.selectionStateShouldBeChanged(index); }
            }
        }

        Item
        {
            id: loginButtonSpacer;
            height: model.loggedToAllServers ? 0 : loginButton.height + SizeManager.spacing.small * 2;
        
            anchors
            {
                left: parent.left;
                right: parent.right;
            }
            
            Rtu.Button
            {
                id: loginButton;
                
                implicitHeight: SizeManager.clickableSizes.base;
                anchors.centerIn: loginButtonSpacer;

                text: qsTr("Enter password to login");
                
                height: model.loggedToAllServers ? 0 : implicitHeight;
                visible: !model.loggedToAllServers;
                
                onClicked: 
                {
                    loginDialog.visible = true;
                }
            }
            
            Dialogs.LoginToSystemDialog
            {
                id: loginDialog;
            }
        }
    }
}
