import QtQuick 2.1;
import QtQuick.Window 2.0;

import "../common" as Common;
import "../controls/base" as Base;
import "." as Dialogs;

import networkoptix.rtu 1.0 as NxRtu;

Dialogs.Dialog
{
    id: thisComponent;

    signal loginClicked(string password);
    signal cancelClicked();

    buttons: NxRtu.Buttons.Ok | NxRtu.Buttons.Cancel;
    styledButtons: NxRtu.Buttons.Ok;
    cancelButton: NxRtu.Buttons.Cancel;

    onButtonClicked:
    {
        switch(id)
        {
        case NxRtu.Buttons.Ok:
            loginClicked(area.password);
            return;
        default:
            cancelClicked();
        }
    }

    title: qsTr("Login to server");

    onVisibleChanged:
    {
        if (!visible)
            return;

        area.showPasswordControl.checked = false;
        area.passwordControl.text = "";
        area.passwordControl.forceActiveFocus();
    }

    areaDelegate: Base.Column
    {
        id: spacer;

        readonly property alias password: passwordEnter.text;
        readonly property alias passwordControl: passwordEnter;
        readonly property alias showPasswordControl: showPasswordCheckbox;

        anchors.centerIn: parent;        
        spacing: Common.SizeManager.spacing.base;

        Row
        {
            id: row;
            
            spacing: Common.SizeManager.spacing.base;
            Base.Text
            {
                anchors.verticalCenter: parent.verticalCenter;

                text: "Password:"
                font.pixelSize: Common.SizeManager.fontSizes.medium;
                width: height * 4;
            }
            
            Base.TextField
            {
                id: passwordEnter;
                
                width: height * 4;
                focus: true;

                echoMode: (showPasswordCheckbox.checked ? TextInput.Normal : TextInput.Password);
                onAccepted:
                {
                    if (!text.trim().length)
                        return;

                    loginClicked(passwordEnter.text);
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
    }     
}
