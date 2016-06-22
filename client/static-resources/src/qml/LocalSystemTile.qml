import QtQuick 2.6;
import QtQuick.Controls 1.2;

import "."

BaseTile
{
    id: thisComponent;

    property var knownHostsModel;
    property var recentUserConnectionsModel;

    // Properties
    property string selectedUser:
    {
        if (isRecentlyConnected)
        {
            if (centralArea)
                return centralArea.userName;
        }
        else if (expandedArea)
        {
            return expandedArea.login;
        }
        else
            return "";
    }

    property bool storePassword: false;
    property bool autoLogin: false;
    property string selectedHost: (centralArea ? centralArea.host : "");
    property string selectedPassword: (expandedArea ? expandedArea.password : "");

    property QtObject activeItemSelector: SingleActiveItemSelector
    {
        variableName: "isMasked";
        deactivateFunc: function(item ) { item.isMaskedPrivate = false; };
    }

    collapseButtonTabItem: centralArea.hostChooseField;

    onIsExpandedChanged:
    {
        if (!isExpanded)
            activeItemSelector.resetCurrentItem();
    }

    centralAreaDelegate: Column
    {
        property alias host: hostChooseItem.value;
        property alias userName: userChooseItem.value;

        property alias userChooseField: userChooseItem;
        property alias hostChooseField: hostChooseItem;

        topPadding: 12; // TODO: paddings do not work??
        spacing: 2;
        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        InfoItem
        {
            id: hostChooseItem;

            isAvailable: thisComponent.allowExpanding && thisComponent.isExpanded;

            disabledLabelColor: Style.colors.midlight;
            enabled: thisComponent.allowExpanding;

            comboBoxTextRole: "display";
            model: thisComponent.knownHostsModel;
            iconUrl: "qrc:/skin/welcome_page/server.png";   // TODO: add ecosystem class for hovered icons
            hoveredIconUrl: "qrc:/skin/welcome_page/server_hover.png";
            disabledIconUrl: "qrc:/skin/welcome_page/server_disabled.png";
            Component.onCompleted: activeItemSelector.addItem(this);

            KeyNavigation.tab: userChooseItem;
            KeyNavigation.backtab: thisComponent.collapseButton;
        }

        InfoItem
        {
            id: userChooseItem;

            isAvailable: thisComponent.allowExpanding && thisComponent.isExpanded;
            visible: thisComponent.isRecentlyConnected;

            disabledLabelColor: Style.colors.midlight;
            enabled: thisComponent.allowExpanding;

            comboBoxTextRole: "userName";
            model: thisComponent.recentUserConnectionsModel;
            iconUrl: "qrc:/skin/welcome_page/user.png";
            hoveredIconUrl: "qrc:/skin/welcome_page/user_hover.png";
            disabledIconUrl: "qrc:/skin/welcome_page/user_disabled.png";

            Component.onCompleted: activeItemSelector.addItem(this);

            KeyNavigation.tab: expandedArea.loginTextItem;
            KeyNavigation.backtab: hostChooseItem;

            onCurrentItemIndexChanged:
            {
                expandedArea.password = ""; // Force clear password field on user change
                updatePasswordData(currentItemIndex);
            }
        }
    }

    expandedAreaDelegate: Column
    {
        property alias login: loginTextField.text;
        property alias password: passwordTextField.text;
        property alias loginTextItem: loginTextField;
        property alias storePasswordCheckbox: savePasswordCheckBox;

        topPadding: 16;
        bottomPadding: 16;
        spacing: 16;

        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        Column
        {
            width: parent.width;

            onVisibleChanged:
            {
                if (loginTextField.visible)
                    loginTextField.forceActiveFocus();
                else
                    passwordTextField.forceActiveFocus();
            }

            spacing: 8;

            NxLabel
            {
                visible: !thisComponent.isRecentlyConnected;
                text: qsTr("Login");
            }

            NxTextEdit
            {
                id: loginTextField;

                visible: !thisComponent.isRecentlyConnected;
                width: parent.width;

                onAccepted: thisComponent.connectClicked();

                KeyNavigation.tab: passwordTextField;
                KeyNavigation.backtab: (centralArea.userChooseField
                    ? centralArea.userChooseField : collapseButton);
            }

            NxLabel
            {
                text: qsTr("Password");
            }

            NxTextEdit
            {
                id: passwordTextField;
                width: parent.width;
                echoMode: TextInput.Password;

                onAccepted: thisComponent.connectClicked();

                KeyNavigation.tab: savePasswordCheckBox;
                KeyNavigation.backtab: loginTextField;
            }

            NxCheckBox
            {
                id: savePasswordCheckBox;
                text: qsTr("Save password");

                onCheckedChanged:
                {
                    if (!checked)
                        autoLoginCheckBoxPrivate.checked = false;

                    thisComponent.storePassword = checked;
                }
            }

            NxCheckBox
            {
                id: autoLoginCheckBoxPrivate;
                enabled: savePasswordCheckBox.checked;
                text: qsTr("Auto-login");

                onCheckedChanged: { thisComponent.autoLogin = checked; }
            }
        }

        NxButton
        {
            anchors.left: parent.left;
            anchors.right: parent.right;

            isAccentButton: true;
            text: qsTr("Connect");

            onClicked: thisComponent.connectClicked();

            KeyNavigation.tab: thisComponent.collapseButton;
        }
    }

    function updatePasswordData(currentItemIndex)
    {
        var hasStoredPasswordValue = (recentUserConnectionsModel
            && recentUserConnectionsModel.getData("hasStoredPassword", currentItemIndex));

        // value can be <undefined>, thus we use explicit conversion here
        var hasStoredPassword = (hasStoredPasswordValue ? true : false);
        if (!expandedArea || !expandedArea.storePasswordCheckbox)
            return;

        expandedArea.storePasswordCheckbox.checked = hasStoredPassword;
        if (!hasStoredPassword)
            return;

        var storedPassword = recentUserConnectionsModel.getData(
            "password", currentItemIndex);
        expandedArea.password = storedPassword;
    }

    Component.onCompleted: { updatePasswordData(0); }
}











