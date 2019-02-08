import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../base" as Base;
import "../rtu" as Rtu;
import "../../common" as Common;
import "../../dialogs" as Dialogs;

import networkoptix.rtu 1.0 as NxRtu;

Item
{
    id: thisComponent;

    property int itemIndex;
    
    property string systemName;
    property int loggedState;
    property int selectedState;
    property bool selectionEnabled: true;
    
    signal selectionStateShouldBeChanged(int currentItemIndex);

    height: column.height + buttonSpacer.height;

    function blink()
    {
        loginButton.blink();
    }

    Base.Column
    {
        id: column;
        
        anchors
        {
            left: parent.left;
            right: parent.right;
            
            leftMargin: Common.SizeManager.spacing.medium;
            rightMargin: Common.SizeManager.spacing.medium;
        }
        
        spacing: 2;
        enabled: thisComponent.selectionEnabled;
        opacity: (enabled ? 1.0 : 0.6);
        Item
        {
            id: headerSpacer;
            width: parent.width;
            height: Common.SizeManager.spacing.base;
        }
        
        Item
        {
            id: descriptionHolder;
            
            height: Math.max(systemNameText.height, selectionCheckbox.height
                , loggedStateComponent.height, Common.SizeManager.clickableSizes.base);
            anchors
            {
                left: parent.left;
                right: parent.right;
            }
             
            Base.CheckBox
            {
                id: selectionCheckbox;
        
                anchors
                {
                    left: parent.left;
                    verticalCenter: parent.verticalCenter;
                }

                activeFocusOnPress: false;
                activeFocusOnTab: false;

                Binding
                {
                    target: selectionCheckbox;
                    property: "checkedState";
                    value: thisComponent.selectedState;
                }
            }
                        
            Base.Text
            {
                id: systemNameText;
    
                anchors
                {
                    left: selectionCheckbox.right;
                    right: loggedStateComponent.left;
                    verticalCenter: parent.verticalCenter;
                    
                    leftMargin: Common.SizeManager.spacing.medium;
                    rightMargin: Common.SizeManager.spacing.medium;
                }
    
                thin: false;
                wrapMode: Text.Wrap;
                font.pixelSize: Common.SizeManager.fontSizes.medium;
                text: (systemName.length === 0 ? qsTr("Unassigned System") : systemName);
            }
            
            Rtu.LoggedState
            {
                id: loggedStateComponent;
                
                anchors
                {
                    right: parent.right;
                    verticalCenter: parent.verticalCenter;
                }
    
                loggedState: thisComponent.loggedState;
            }
        }
    
        Base.LineDelimiter
        {
            color: "#666666";
        }
    }
        
    MouseArea
    {
        id: selectionMouseArea;

        anchors
        {
            left: parent.left;
            right: parent.right;
            top: column.top;
            bottom: column.bottom;
        }

        onClicked: { thisComponent.selectionStateShouldBeChanged(index); }
    }
    
    Item
    {
        id: buttonSpacer;
        
        height: (loginButton.visible ? loginButton.height + Common.SizeManager.spacing.base : 0);
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: column.bottom;

            leftMargin: Common.SizeManager.spacing.medium;
            rightMargin: Common.SizeManager.spacing.medium;
        }
        
        Base.StyledButton
        {
            id: loginButton;
            
            visible: (thisComponent.loggedState !== NxRtu.Constants.LoggedToAllServers);

            anchors
            {
                verticalCenter: parent.verticalCenter;
                left:(visible ? parent.left : undefined);
                right: (visible ? parent.right : undefined);
            }
        
            activeFocusOnTab: false;
            activeFocusOnPress: false;

            text: qsTr("Enter the Password");
            fontSize: Common.SizeManager.fontSizes.base;
            onClicked: { loginDialog.show(); }
            
            Dialogs.LoginToSystemDialog
            {
                id: loginDialog;

                onLoginClicked: { rtuContext.tryLoginWith(thisComponent.systemName, password); }
            }
        }
    }
}

