import QtQuick 2.6;
import QtQuick.Controls 1.2;

import "."

BaseTile
{
    id: thisComponent;

    property var knownUsersModel;
    property var knownHostsModel;

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

    property string selectedHost: (centralArea ? centralArea.host : "");
    property string selectedPassword: (expandedArea ? expandedArea.password : "");

    property QtObject activeItemSelector: SingleActiveItemSelector
    {
        variableName: "isMasked";
        writeVariableName: "isMaskedPrivate";
        onIsSomeoneActiveChanged:
        {
            if (isSomeoneActive)
                isExpandedPrivate = true;
        }
    }

    onIsExpandedChanged:
    {
        if (!isExpanded)
            activeItemSelector.resetCurrentItem();
    }

    centralAreaDelegate: Column
    {
        property alias host: hostChooseItem.value;
        property alias userName: userChooseItem.value;

        topPadding: 12;
        spacing: 2;
        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        InfoItem
        {
            id: hostChooseItem;

            isAvailable: thisComponent.correctTile && thisComponent.isExpanded;

            model: thisComponent.knownHostsModel;
            iconUrl: (thisComponent.correctTile ? "non_empty_url" : "");// TODO: change to proper url

            textColor: Style.colors.windowText;
            textFont: Style.fonts.systemTile.info;

            Component.onCompleted: activeItemSelector.addItem(this);
        }

        InfoItem
        {
            id: userChooseItem;

            visible: thisComponent.isRecentlyConnected;
            isAvailable: thisComponent.correctTile && thisComponent.isExpanded;

            model: thisComponent.knownUsersModel;
            comboBoxTextRole: "userName";

            textColor: Style.colors.windowText;
            textFont: Style.fonts.systemTile.info;

            Component.onCompleted: activeItemSelector.addItem(this);
        }
    }

    expandedAreaDelegate: Column
    {
        property alias login: loginTextField.text;
        property alias password: passwordTextField.text;

        topPadding: 16;
        bottomPadding: 16;
        spacing: 16;

        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        Column
        {
            width: parent.width;

            spacing: 8;
            Text
            {
                visible: !thisComponent.isRecentlyConnected;
                text: qsTr("Login");

                font: Style.label.font;
                color: Style.label.color;
            }

            TextField
            {
                id: loginTextField;

                visible: !thisComponent.isRecentlyConnected;
                width: parent.width;
            }

            Text
            {
                text: qsTr("Password");
                font: Style.label.font;
                color: Style.label.color;
            }

            TextField
            {
                id: passwordTextField;
                width: parent.width;
            }

            CheckBox
            {
                text: qsTr("Save password");
            }

            CheckBox
            {
                enabled: false;
                text: qsTr("Auto-login");
            }
        }

        Button
        {
            anchors.left: parent.left;
            anchors.right: parent.right;

            text: qsTr("Connect");

            onClicked: thisComponent.connectClicked();
        }
    }
}











