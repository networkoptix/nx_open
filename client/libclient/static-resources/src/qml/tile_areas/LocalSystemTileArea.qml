import QtQuick 2.6;
import QtQuick.Controls 1.2;

import ".."

Item
{
    id: control;

    property bool isExpandedTile: false;
    property real expandedOpacity: 0;

    property string selectedHost: hostChooseItem.value;
    property string selectedUser: (control.impl.hasRecentConnections ?
        userChooseItem.value : expandedArea.loginTextField.text);
    property string selectedPassword: expandedArea.passwordTextField.text;
    property bool savePassword: expandedArea.savePasswordCheckbox.checked;
    property bool autoLogin: expandedArea.autoLoginCheckBox.checked;

    property var hostsModel;
    property var recentUserConnectionsModel;

    property var prevTabObject;

    property QtObject activeItemSelector: SingleActiveItemSelector
    {
        variableName: "isMasked";
        deactivateFunc: function(item ) { item.isMaskedPrivate = false; };
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
        target: (recentUserConnectionsModel ? recentUserConnectionsModel : null);
        onConnectionDataChanged: { control.impl.updatePasswordData(index); }
    }

    Column
    {
        id: collapsedArea;

        topPadding: 12;
        anchors.left: parent.left;
        anchors.right: parent.right;

        InfoItem
        {
            id: hostChooseItem;

            model: control.hostsModel;

            isAvailable: enabled && control.isExpandedTile;

            disabledLabelColor: Style.colors.midlight;

            comboBoxTextRole: "display";
            iconUrl: "qrc:/skin/welcome_page/server.png";                   // TODO: add ecosystem class for hovered icons
            hoveredIconUrl: "qrc:/skin/welcome_page/server_hover.png";
            disabledIconUrl: "qrc:/skin/welcome_page/server_disabled.png";
            Component.onCompleted: activeItemSelector.addItem(this);

            KeyNavigation.tab: userChooseItem;
            KeyNavigation.backtab: (prevTabObject ? prevTabObject : null);
        }

        InfoItem
        {
            id: userChooseItem;

            model: control.recentUserConnectionsModel;

            isAvailable: enabled && control.isExpandedTile;
            visible: control.impl.hasRecentConnections;

            disabledLabelColor: Style.colors.midlight;

            comboBoxTextRole: "userName";
            iconUrl: "qrc:/skin/welcome_page/user.png";
            hoveredIconUrl: "qrc:/skin/welcome_page/user_hover.png";
            disabledIconUrl: "qrc:/skin/welcome_page/user_disabled.png";

            Component.onCompleted: activeItemSelector.addItem(this);

            KeyNavigation.tab: expandedArea.loginTextField;
            KeyNavigation.backtab: hostChooseItem;

            onCurrentItemIndexChanged:
            {
                expandedArea.passwordTextField.text = ""; // Force clear password field on user change
                control.impl.updatePasswordData(currentItemIndex);
            }

            onValueChanged:
            {
                expandedArea.loginTextField.text = value;
            }
        }
    }

    LocalSystemTileExpandedAreaPart
    {
        id: expandedArea;

        anchors.top: collapsedArea.bottom;
        visible: control.isExpandedTile;
        enabled: (control.isExpandedTile);
        opacity: control.expandedOpacity;
        hasRecentConnections: control.impl.hasRecentConnections;

        prevTabObject: (userChooseItem ? userChooseItem : control.prevTabObject);
        nextTabObject: control.prevTabObject;

        onConnectButtonClicked: { control.connectRequested(); }
    }


    property QtObject impl: QtObject
    {
        readonly property bool hasRecentConnections: userChooseItem.value.length;

        function updatePasswordData(currentItemIndex)
        {
            if (currentItemIndex !== userChooseItem.currentItemIndex)
                return; // Do not update if it is not current item

            var hasStoredPasswordValue = (control.recentUserConnectionsModel
                && control.recentUserConnectionsModel.getData("hasStoredPassword", currentItemIndex));

            // value can be <undefined>, thus we use explicit conversion here
            var hasStoredPassword = (hasStoredPasswordValue ? true : false);
            expandedArea.savePasswordCheckbox.checked = hasStoredPassword;

            expandedArea.passwordTextField.text = (hasStoredPassword ?
                recentUserConnectionsModel.getData("password", currentItemIndex) : "")
        }
    }


    Component.onCompleted: { impl.updatePasswordData(0); }
}
