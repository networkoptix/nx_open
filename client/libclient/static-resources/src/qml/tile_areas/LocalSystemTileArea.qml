import QtQuick 2.6;
import Nx.Models 1.0;

import ".."

Item
{
    id: control;

    property bool isExpandedTile: false;
    property real expandedOpacity: 0;
    property alias factorySystem: expandedArea.factorySystem;

    property string localId;
    property string selectedHost: hostChooseItem.value;
    property string selectedUser: (control.impl.hasRecentConnections ?
        userChooseItem.value : expandedArea.loginTextField.text);
    property string selectedPassword: expandedArea.passwordTextField.text;
    property bool savePassword: expandedArea.savePasswordCheckbox.checked;
    property bool autoLogin: expandedArea.autoLoginCheckBox.checked;
    property bool isConnecting: false;
    property var hostsModel;
    property var authenticationDataModel;

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
        target: authDataAccessor;
        onDataChanged: { control.impl.updatePasswordData(startRow); }
    }

    Column
    {
        id: collapsedArea;

        topPadding: 12;
        anchors.left: parent.left;
        anchors.right: parent.right;

        spacing: (hostChooseItem.isMasked || userChooseItem.isMasked ? 8 : 2);

        InfoItem
        {
            id: hostChooseItem;

            model: control.hostsModel;

            isAvailable: enabled && control.isExpandedTile && !control.isConnecting;

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

            model: control.authenticationDataModel;

            Connections
            {
                target: authDataAccessor;
                onDataChanged:
                {
                    if (startRow == 0)
                        userChooseItem.forceCurrentIndex(0); // Resets user to first.
                }
                onModelChanged:
                {
                    userChooseItem.forceCurrentIndex(userChooseItem.currentItemIndex);
                }
            }

            isAvailable: enabled && control.isExpandedTile  && !control.isConnecting;
            visible: control.impl.hasRecentConnections;

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
        Connections
        {
            target: expandedArea.savePasswordCheckbox;
            onClicked:
            {
                var previosStateIsChecked = expandedArea.savePasswordCheckbox.checked;
                if (previosStateIsChecked) //< It means next state after click or "space" is "unchecked"
                    context.forgetPassword(control.localId, control.selectedUser);
            }
        }
    }

    ModelDataAccessor
    {
        id: authDataAccessor;
        model: authenticationDataModel || null;
    }

    property QtObject impl: QtObject
    {
        readonly property bool hasRecentConnections: authDataAccessor.count > 0;

        function updatePasswordData(currentItemIndex)
        {
            if (currentItemIndex !== userChooseItem.currentItemIndex)
                return; // Do not update if it is not current item

            var credentials = authDataAccessor.getData(currentItemIndex, "credentials");
            var password = credentials && credentials.password;
            expandedArea.savePasswordCheckbox.checked = !!password;
            expandedArea.passwordTextField.text = password || "";
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
