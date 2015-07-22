import QtQuick 2.1;
import QtQuick.Window 2.0;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/rtu" as Rtu;

Window
{
    id: thisComponent;

    property var serverId;

    signal okClicked();
    signal cancelClicked();
    
    flags: Qt.WindowTitleHint | Qt.MSWindowsFixedSizeDialogHint;
    
    title: qsTr("Login to server");

    width: spacer.width + Common.SizeManager.spacing.medium;
    height: spacer.height + Common.SizeManager.spacing.medium;
    
    onVisibleChanged:
    {
        if (visible)
        {
            password.text = "";
            showPasswordCheckbox.checked = false;
            password.focus = true;
        }
    }

    Base.Column
    {
        id: spacer;

        anchors.centerIn: parent;        
        spacing: Common.SizeManager.spacing.base;
        Row
        {
            id: row;
            
            spacing: Common.SizeManager.spacing.base;
            Base.Text
            {
                text: "Password:"
                anchors.verticalCenter: parent.verticalCenter;
                width: height * 4;
            }
            
            Base.TextField
            {
                id: password;
                
                width: height * 4;
                focus: true;

                echoMode: (showPasswordCheckbox.checked ? TextInput.Normal : TextInput.Password);
                onAccepted:
                {
                    okClicked();
                    thisComponent.close();
                }
            }
        }
        
        Base.CheckBox
        {
            id: showPasswordCheckbox;

            anchors.right: parent.right;
            
            text: qsTr("Show password");
        }

        Row
        {
            layoutDirection: Qt.RightToLeft;
            
            spacing: Common.SizeManager.spacing.base;
            anchors.right: parent.right;
            
            Base.StyledButton
            {
                id: okButton;
                text: qsTr("Ok");

                width: height * 3;
                
                onClicked:
                {
                    okClicked();
                    thisComponent.close();
                }                    
            }
            
            Base.Button
            {
                text: qsTr("Cancel");
                width: height * 3;
                
                onClicked:
                {
                    cancelClicked();
                    thisComponent.close();
                }
            }
        }
    }     
    
    onOkClicked: 
    {
        rtuContext.tryLoginWith(password.text);
    }
}
