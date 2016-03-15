import QtQuick 2.5;
import QtQuick.Controls 1.2;

BaseTile
{
    id: thisComponent;

    property string host;
    property string userName;

    property var knownUsersModel;
    property var knownHostsModel;

    // Properties
    property string selectedUser: (centralArea ? centralArea.userName : "");
    property string selectedHost: (centralArea ? centralArea.host : "");
    property string selectedPassword: (expandedArea ? expandedArea.password : "");

    signal connectClicked;

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
        property alias host: hostChooseItem.text;
        property alias userName: userChooseItem.text;

        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        InfoItem
        {
            id: hostChooseItem;

            isAvailable: thisComponent.correctTile;

            text: thisComponent.host;
            model: thisComponent.knownHostsModel;
            iconUrl: (thisComponent.correctTile ? "non_empty_url" : "");// TODO: change to proper url

            Component.onCompleted: activeItemSelector.addItem(this);
        }

        InfoItem
        {
            id: userChooseItem;

            isAvailable: thisComponent.correctTile;

            text: thisComponent.userName;
            model: thisComponent.knownUsersModel;

            Component.onCompleted: activeItemSelector.addItem(this);
        }
    }

    expandedAreaDelegate: Column
    {
        property alias password: passwordTextField.text;

        spacing: 8;

        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        Text
        {
            text: qsTr("Password");
        }

        TextField
        {
            id: passwordTextField;
            anchors.left: parent.left;
            anchors.right: parent.right;

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











