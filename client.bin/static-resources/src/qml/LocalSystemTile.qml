import QtQuick 2.5;
import QtQuick.Controls 1.2;

BaseTile
{
    id: thisComponent;

    property string host;
    property string userName;
    property bool isComaptible;

//    property alias selectedUserName: userNameTextEdit.text;
//    property alias selectedPassword: passwordTextField.text;
    signal onConnectClicked;

    property QtObject activeItemSelector: SingleActiveItemSelector
    {
        variableName: "isMasked";
        onIsSomeoneActiveChanged:
        {
            if (isSomeoneActive)
                isExpanded = true;
        }
    }

    onIsExpandedChanged:
    {
        if (!isExpanded)
            activeItemSelector.resetCurrentItem();
    }

    centralAreaDelegate: Column
    {
        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        InfoItem
        {
            text: thisComponent.host;
            iconUrl: (isComaptible ? "non_empty_url" : "");// TODO: change to proper url

            Component.onCompleted: activeItemSelector.addItem(this);    // TODO: add ActiveWatchable qml component?
        }

        InfoItem
        {
            id: userNameTextEdit;
            text: "user";//thisComponent.userName;

            Component.onCompleted: activeItemSelector.addItem(this);
        }
    }

    expandedAreaDelegate: Column
    {
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

            onClicked: thisComponent.onConnectClicked();
        }
    }
}











