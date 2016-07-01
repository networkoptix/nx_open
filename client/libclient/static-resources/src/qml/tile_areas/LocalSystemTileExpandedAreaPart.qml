import QtQuick 2.6;
import QtQuick.Controls 1.2;

import ".."

Column
{
    id: control;

    property bool hasRecentConnections: false;

    property alias loginTextField: loginTextItem;
    property alias passwordTextField: passwordTextItem;

    property alias savePasswordCheckbox: savePasswordCheckBoxControl;
    property alias autoLoginCheckBox: autoLoginCheckBoxItem;

    property var prevTabObject;
    property var nextTabObject;

    topPadding: 16;
    bottomPadding: 16;
    spacing: 16;

    anchors.left: (parent ? parent.left : undefined);
    anchors.right: (parent ? parent.right : undefined);

    signal connectButtonClicked();

    Column
    {
        width: parent.width;

        onVisibleChanged:
        {
            if (loginTextItem.visible)
                loginTextItem.forceActiveFocus();
            else
                passwordTextItem.forceActiveFocus();
        }

        spacing: 8;

        NxLabel
        {
            visible: !control.hasRecentConnections;
            text: qsTr("Login");
        }

        NxTextEdit
        {
            id: loginTextItem;

            visible: !control.hasRecentConnections;
            width: parent.width;

            onAccepted: control.connectClicked();

            KeyNavigation.tab: passwordTextItem;
            KeyNavigation.backtab: prevTabObject;
        }

        NxLabel
        {
            text: qsTr("Password");
        }

        NxTextEdit
        {
            id: passwordTextItem;

            width: parent.width;
            echoMode: TextInput.Password;

            onAccepted: control.connectButtonClicked();

            KeyNavigation.tab: savePasswordCheckBoxControl;
            KeyNavigation.backtab: loginTextItem;
        }

        NxCheckBox
        {
            id: savePasswordCheckBoxControl;
            text: qsTr("Save password");

            onCheckedChanged:
            {
                if (!checked)
                    autoLoginCheckBoxItem.checked = false;
            }
        }

        NxCheckBox
        {
            id: autoLoginCheckBoxItem;
            enabled: savePasswordCheckBoxControl.checked;
            text: qsTr("Auto-login");
        }
    }

    NxButton
    {
        anchors.left: parent.left;
        anchors.right: parent.right;

        isAccentButton: true;
        text: qsTr("Connect");

        KeyNavigation.tab: control.nextTabObject;

        onClicked: control.connectButtonClicked();
    }
}
