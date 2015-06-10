import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../base" as Base;
import "../../common" as Common;
import "../../dialogs" as Dialogs;

Item
{
    id: thisComponent;

    signal selectionStateShouldBeChanged(int currentItemIndex);

    height: topColumn.height;
/*
    anchors
    {
        left: parent.left;
        right: parent.right;
    }
    */

    Base.Column
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

            Item
            {
                id: column;

                height: spacer.height + descriptionRow.height + delimiter.height;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                }

                Item
                {
                    id: spacer;
        
                    height: Common.SizeManager.spacing.base;
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
                        top: spacer.bottom;
                        left: parent.left;
                        right: parent.right;
                    }
        
                    Item
                    {
                        id: selectionCheckboxWrapper;
        
                        width: Common.SizeManager.clickableSizes.medium;
                        height: width;
                    
                        anchors.verticalCenter: parent.verticalCenter;
        
                        Base.CheckBox
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
        
                    Base.Text
                    {
                        id: systemName;
        
                        height: Common.SizeManager.clickableSizes.base;
        
                        anchors
                        {
                            left: selectionCheckboxWrapper.right;
                            right: parent.right;
                            verticalCenter: parent.verticalCenter;

                        }
        
                        font.pointSize: Common.SizeManager.fontSizes.large;
                        text: (!model.name || (model.name.length === 0) ? qsTr("NO SYSTEM") : model.name);
                    }
                }
        
                Base.LineDelimiter
                {
                    id: delimiter;
                    thinDelimiter: false;
                    color: "grey";

                    anchors.top: column.bottom;
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
            height: model.loggedToAllServers ? 0 : loginButton.height + Common.SizeManager.spacing.small * 2;
        
            anchors
            {
                left: parent.left;
                right: parent.right;
            }
            
            Base.Button
            {
                id: loginButton;
                
                implicitHeight: Common.SizeManager.clickableSizes.base;
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
