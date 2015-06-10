import QtQuick 2.1;
import QtQuick.Window 2.0;

import "../common" as Common;
import "../controls/base" as Base;

Window
{
    id: thisComponent;

    property var serverId;

    signal okClicked();
    signal cancelClicked();
    
    flags: Qt.Dialog | Qt.MSWindowsFixedSizeDialogHint;
    
    title: qsTr("Login to server");

    width: spacer.width + Common.SizeManager.spacing.base;
    height: spacer.height + Common.SizeManager.spacing.base
    
    Base.Column
    {
        id: spacer;

        anchors.centerIn: parent;        

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
            }
        }
        
        Base.CheckBox
        {
            anchors.right: parent.right;
            
            text: qsTr("Show password");
        }

        Row
        {
            layoutDirection: Qt.RightToLeft;
            
            spacing: Common.SizeManager.spacing.base;
            anchors.right: parent.right;
            
            Base.Button
            {
                text: qsTr("Ok");
                width: height * 2.5;
                
                onClicked:
                {
                    okClicked();
                    thisComponent.close();
                }                    
            }
            
            Base.Button
            {
                text: qsTr("Cancel");
                width: height * 2.5;
                
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
