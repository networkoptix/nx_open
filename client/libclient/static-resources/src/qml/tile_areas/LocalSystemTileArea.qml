import QtQuick 2.6;

import ".."

Item
{
    id: control;

    property bool isOnline: false;
    property bool isExpandedTile: false;
    property real expandedOpacity: 0;

    property string selectedHost: hostChooseItem.value;
    property string selectedUser: (control.impl.hasRecentConnections ?
        userChooseItem.value : expandedArea.loginTextField.text);
    property string selectedPassword: expandedArea.passwordTextField.text;
    property bool savePassword: expandedArea.savePasswordCheckbox.checked;
    property bool autoLogin: expandedArea.autoLoginCheckBox.checked;
    property bool isConnecting: false;
    property var hostsModel;
    property var recentLocalConnectionsModel;

    property var prevTabObject;

    property QtObject activeItemSelector: SingleActiveItemSelector
    {
        variableName: "isMasked";
        deactivateFunc: function(item) { item.isMaskedPrivate = false; };
    }

    signal connectRequested();

    height: collapsedArea.height + expandedArea.height;

    anchors.left: (parent ? parent.left : undefined);
    anchors.right: (parent ? parent.right : undefined);

    onPrevTabObjectChanged:
    {
        if (prevTabObject)
            prevTabObject.KeyNavigation.tab = hostChooseItem;
    }

    onExpandedOpacityChanged:
    {
        if (expandedOpacity != 1)
            activeItemSelector.resetCurrentItem();
    }

    Binding
    {
        target: (expandedArea ? expandedArea.autoLoginCheckBox : null);
        property: "checked";
        value: control.autoLogin;
    }

    Binding
    {
        target: control;
        property: "autoLogin";
        value: (expandedArea ? expandedArea.autoLoginCheckBox.checked : false);
    }

    Connections
    {
        target: (recentLocalConnectionsModel ? recentLocalConnectionsModel : null);
        onConnectionDataChanged: { control.impl.updatePasswordData(index); }
    }

    Column
    {
        id: collapsedArea;

        topPadding: 12;
        anchors.left: parent.left;
        anchors.right: parent.right;

        spacing: (hostChooseItem.isMasked || userChooseItem.isMasked ? 8 : 0);

        InfoItem
        {
            id: hostChooseItem;

            model: control.hostsModel;

            isAvailable: enabled && control.isExpandedTile && !control.isConnecting;

            comboBoxTextRole: "display";
            comboBoxValueRole: "url";
            iconUrl: "qrc:/skin/welcome_page/server.png";                   // TODO: add ecosystem class for hovered icons
            hoveredIconUrl: "qrc:/skin/welcome_page/server_hover.png";
            disabledIconUrl: "qrc:/skin/welcome_page/server_disabled.png";
            hoverExtraIconUrl: "qrc:/skin/welcome_page/edit.png";

            Component.onCompleted: activeItemSelector.addItem(this);

            KeyNavigation.tab: userChooseItem;
            KeyNavigation.backtab: (prevTabObject ? prevTabObject : null);

            onAccepted: control.connectRequested();

        }

        InfoItem
        {
            id: userChooseItem;

            model: control.recentLocalConnectionsModel;

            Connections
            {
                target: userChooseItem.model;
                ignoreUnknownSignals: true;
                onFirstUserChanged: userChooseItem.forceCurrentIndex(0);    //< Resets user to first
            }

            isAvailable: enabled && control.isExpandedTile  && !control.isConnecting;
            visible: control.impl.hasRecentConnections;

            comboBoxTextRole: "userName";
            iconUrl: "qrc:/skin/welcome_page/user.png";
            hoveredIconUrl: "qrc:/skin/welcome_page/user_hover.png";
            disabledIconUrl: "qrc:/skin/welcome_page/user_disabled.png";
            hoverExtraIconUrl: "qrc:/skin/welcome_page/edit.png";

            Component.onCompleted: activeItemSelector.addItem(this);

            KeyNavigation.tab: expandedArea.loginTextField;
            KeyNavigation.backtab: hostChooseItem;

            onCurrentItemIndexChanged:
            {
                control.impl.updatePasswordData(currentItemIndex);
            }

            onValueChanged:
            {
                expandedArea.loginTextField.text = value;
            }

            onAccepted: control.connectRequested();
        }
    }

    LocalSystemTileExpandedAreaPart
    {
        id: expandedArea;

        isConnecting: control.isConnecting;
        anchors.top: collapsedArea.bottom;
        visible: control.isExpandedTile;
        enabled: control.isExpandedTile;
        opacity: control.expandedOpacity;
        hasRecentConnections: control.impl.hasRecentConnections;

        prevTabObject: (userChooseItem.visible ? userChooseItem : hostChooseItem);
        nextTabObject: control.prevTabObject;

        onConnectButtonClicked: { control.connectRequested(); }
    }

    property QtObject impl: QtObject
    {
        readonly property bool hasRecentConnections: (recentLocalConnectionsModel && recentLocalConnectionsModel.hasConnections);

        function updatePasswordData(currentItemIndex)
        {
            if (currentItemIndex !== userChooseItem.currentItemIndex)
                return; // Do not update if it is not current item

            if (currentItemIndex == -1) //< In case of non-existent user
            {
                expandedArea.savePasswordCheckbox.checked = false;  // Reset "Store password" checkbox
                expandedArea.passwordTextField.text = "";
                return;
            }

            var hasStoredPasswordValue = (control.recentLocalConnectionsModel
                && control.recentLocalConnectionsModel.getData("hasStoredPassword", currentItemIndex));

            // value can be <undefined>, thus we use explicit conversion here
            var hasStoredPassword = (hasStoredPasswordValue ? true : false);
            expandedArea.savePasswordCheckbox.checked = hasStoredPassword;

            expandedArea.passwordTextField.text = (hasStoredPassword ?
                recentLocalConnectionsModel.getData("password", currentItemIndex) : "")
        }
    }

    onIsExpandedTileChanged:
    {
        if (isExpandedTile)
            return;

        if (userChooseItem.currentItemIndex == -1)
        {
            // Clears password if it is new user.
            // Is "save password" is checked then connection will be stored anyway
            expandedArea.passwordTextField.text = "";
            expandedArea.savePasswordCheckbox.checked = false;
        }

        impl.updatePasswordData(userChooseItem.currentItemIndex);
    }
    Component.onCompleted: { impl.updatePasswordData(0); }
}
