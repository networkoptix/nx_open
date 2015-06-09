import QtQuick 2.4;
import QtQuick.Window 2.0;

import "../controls" as Common;
import "../controls/standard" as Rtu;

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
    
    Column
    {
        id: spacer;

        anchors.centerIn: parent;        
        spacing: Common.SizeManager.spacing.base;
        
        Row
        {
            id: row;
            
            spacing: Common.SizeManager.spacing.base;
            Rtu.Text
            {
                text: "Password:"
                anchors.verticalCenter: parent.verticalCenter;
                width: height * 4;
            }
            
            Rtu.TextField
            {
                id: password;
                
                width: height * 4;
                focus: true;
            }
        }
        
        Rtu.CheckBox
        {
            anchors.right: parent.right;
            
            text: qsTr("Show password");
        }

        Row
        {
            layoutDirection: Qt.RightToLeft;
            
            spacing: Common.SizeManager.spacing.base;
            anchors.right: parent.right;
            
            Rtu.Button
            {
                text: qsTr("Ok");
                width: height * 2.5;
                
                onClicked:
                {
                    okClicked();
                    thisComponent.close();
                }                    
            }
            
            Rtu.Button
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
