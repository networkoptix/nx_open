import QtQuick 2.5;
import QtQuick.Controls 1.2;

BaseTile
{
    id: thisComponent;

    property string userName;

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

        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        InfoItem
        {
            id: hostChooseItem;

            isAvailable: thisComponent.correctTile;

            value: thisComponent.host;
            model: thisComponent.knownHostsModel;
            iconUrl: (thisComponent.correctTile ? "non_empty_url" : "");// TODO: change to proper url

            Component.onCompleted: activeItemSelector.addItem(this);
        }

        InfoItem
        {
            id: userChooseItem;

            isAvailable: thisComponent.correctTile;

            value: thisComponent.userName;
            model: thisComponent.knownUsersModel;

            Component.onCompleted: activeItemSelector.addItem(this);

            visible: thisComponent.isRecentlyConnected;
        }
    }

    expandedAreaDelegate: Column
    {
        property alias login: loginTextField.text;
        property alias password: passwordTextField.text;

        spacing: 8;

        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        Text
        {
            visible: !thisComponent.isRecentlyConnected;
            text: qsTr("Login");
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

        Button
        {
            anchors.left: parent.left;
            anchors.right: parent.right;

            text: qsTr("Connect");

            onClicked: thisComponent.connectClicked();
        }
    }
}











