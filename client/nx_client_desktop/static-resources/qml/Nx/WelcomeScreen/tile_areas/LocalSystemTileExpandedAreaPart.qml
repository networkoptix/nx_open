import QtQuick 2.6;
import Nx 1.0;

import ".."

Column
{
    id: control;

    property bool isConnecting: false;
    property bool hasRecentConnections: false;
    property bool factorySystem: false;

    property alias loginTextField: loginTextItem;
    property alias passwordTextField: passwordTextItem;

    property alias savePasswordCheckbox: savePasswordCheckBoxControl;
    property alias autoLoginCheckBox: autoLoginCheckBoxItem;

    property var prevTabObject;
    property var nextTabObject;

    topPadding: 16;
    bottomPadding: 16;
    spacing: 16;

    opacity: 0;
    anchors.left: (parent ? parent.left : undefined);
    anchors.right: (parent ? parent.right : undefined);

    signal connectButtonClicked();

    onOpacityChanged:
    {
        if (opacity < 1.0 || !visible)
            return;

        passwordTextItem.forceActiveFocus();
    }

    Column
    {
        width: parent.width;

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
            text: "admin";
            width: parent.width;

            onAccepted: control.connectButtonClicked();

            KeyNavigation.tab: passwordTextItem;
            KeyNavigation.backtab: prevTabObject;

            enabled: !control.isConnecting;
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
            KeyNavigation.backtab: (loginTextItem.visible ? loginTextItem : control.prevTabObject);

            enabled: !control.isConnecting;
            onEnabledChanged:
            {
                if (enabled)
                    forceActiveFocus();
            }
         }

        NxCheckBox
        {
            id: savePasswordCheckBoxControl;
            text: qsTr("Save password");

            enabled: !control.isConnecting && !control.factorySystem;
            onAccepted: control.connectButtonClicked();

            onCheckedChanged:
            {
                if (!checked)
                {
                    passwordTextItem.text = "";
                    autoLoginCheckBoxItem.checked = false;
                }
            }

            KeyNavigation.tab: autoLoginCheckBoxItem;
            KeyNavigation.backtab: passwordTextField;
        }

        NxCheckBox
        {
            id: autoLoginCheckBoxItem;
            enabled: savePasswordCheckBoxControl.enabled
                && savePasswordCheckBoxControl.checked
                && !control.isConnecting;

            text: qsTr("Auto-login");

            onAccepted: control.connectButtonClicked();

            KeyNavigation.tab: connectButton;
            KeyNavigation.backtab: savePasswordCheckBoxControl;
        }
    }

    NxButton
    {
        id: connectButton;
        anchors.left: parent.left;
        anchors.right: parent.right;

        isAccentButton: true;

        KeyNavigation.tab: control.nextTabObject;
        KeyNavigation.backtab: autoLoginCheckBoxItem;

        onClicked: { control.connectButtonClicked(); }

        text: (control.isConnecting ? "" : qsTr("Connect"));
        enableHover: !control.isConnecting;

        MouseArea
        {
            id: cancelButtonBehaviourArea;
            anchors.fill: parent;
            visible: control.isConnecting;
        }

        NxDotPreloader
        {
            color: ColorTheme.brightText;
            visible: control.isConnecting;

            anchors.centerIn: parent;
        }
    }
}
