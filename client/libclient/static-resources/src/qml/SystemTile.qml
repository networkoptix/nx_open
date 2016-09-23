
import QtQuick 2.6;
import NetworkOptix.Qml 1.0;

import "."

BaseTile
{
    id: control;

    property string systemName;
    property string ownerDescription;

    property string systemId;
    property bool isFactoryTile: false;
    property bool isCloudTile: false;
    property bool isCompatibleInternal: false;

    property string wrongVersion;
    property string compatibleVersion;

    isConnecting: ((control.systemId == context.connectingToSystem) && !isFactoryTile);

    isAvailable:
    {
        if (isFactoryTile)
            return true;

        if (wrongVersion.length || !isCompatibleInternal)
            return false;

        return control.impl.isOnline;
    }

    tileColor:
    {
        if (isFactoryTile)
        {
            return (isHovered ? Style.colors.custom.systemTile.factorySystemHovered :
                Style.colors.custom.systemTile.factorySystemBkg);
        }

        if (!control.isAvailable)
            return Style.colors.custom.systemTile.disabled;

        if (control.isHovered)
            return Style.colors.custom.systemTile.backgroundHovered;

        return Style.colors.custom.systemTile.background;
    }

    indicator
    {
        visible: ((impl.tileType !== impl.kFactorySystemTileType) &&
            (wrongVersion.length ||
             compatibleVersion.length || !impl.isOnline || !isCompatibleInternal));

        text:
        {
            if (!isCompatibleInternal)
                return qsTr("INCOMPATIBLE");
            if (wrongVersion.length)
                return wrongVersion;
            if (compatibleVersion.length)
                return compatibleVersion;
            if (!impl.isOnline)
                return qsTr("OFFLINE");

            return "";
        }

        textColor:
        {
           if (wrongVersion.length ||
                compatibleVersion.length || !isCompatibleInternal)
           {
               return Style.colors.shadow;
           }
           else
               return Style.colors.windowText;
        }

        color:
        {
            if (wrongVersion.length || !isCompatibleInternal)
                return Style.colors.red_main;
            else if (compatibleVersion.length)
                return Style.colors.yellow_main;
            else
                return Style.colors.custom.systemTile.offlineIndicatorBkg;
        }
    }

    onCollapsedTileClicked:
    {
        if (!control.isAvailable)
            return;

        if (control.isFactoryTile)
        {
            var factorySystemHost = areaLoader.item.host;
            console.log("Show wizard for system <", systemName,
                ">, host <", factorySystemHost, ">");
            context.setupFactorySystem(factorySystemHost);
        }
        else if (isCloudTile)
        {
            var cloudHost = control.impl.hostsModel.firstHost;
            console.log("Connecting to cloud system <", systemName,
                ">, through the host <", cloudHost, ">");
            context.connectToCloudSystem(control.systemId, cloudHost);
        }
        else // Local system tile
        {
            if (impl.hasSavedConnection)
                control.impl.connectToLocalSystem();
            else
                toggle();
        }
    }

    titleLabel.text: (isFactoryTile ? qsTr("New System") : systemName);

    menuButton
    {
        visible: impl.hasSavedConnection && impl.isOnline;

        menu: NxPopupMenu
        {
            NxMenuItem
            {
                text: "Edit";
                leftPadding: 16;
                rightPadding: 16;

                onTriggered: control.toggle();
            }
        }
    }

    areaLoader.source:
    {
        if (isFactoryTile)
            return "tile_areas/FactorySystemTileArea.qml";
        else if (isCloudTile)
            return "tile_areas/CloudSystemTileArea.qml";
        else
            return "tile_areas/LocalSystemTileArea.qml";
    }

    Connections
    {
        target: areaLoader;

        onItemChanged:
        {
            var currentAreaItem = control.areaLoader.item;
            if (!currentAreaItem)
                return;

            if (control.impl.tileType === control.impl.kLocalSystemTileType)
            {
                currentAreaItem.isOnline = Qt.binding( function() { return control.impl.isOnline; });
                currentAreaItem.isExpandedTile = Qt.binding( function() { return control.isExpanded; });
                currentAreaItem.expandedOpacity = Qt.binding( function() { return control.expandedOpacity; });
                currentAreaItem.hostsModel = control.impl.hostsModel;
                currentAreaItem.recentLocalConnectionsModel = control.impl.recentConnectionsModel;
                currentAreaItem.enabled = Qt.binding( function () { return control.isAvailable; });
                currentAreaItem.prevTabObject = Qt.binding( function() { return control.collapseButton; });
                currentAreaItem.isConnecting = Qt.binding( function() { return control.isConnecting; });
            }
            else if (control.impl.tileType === control.impl.kFactorySystemTileType)
            {
                currentAreaItem.host = Qt.binding( function()
                {
                    return (control.impl.hostsModel ?
                        control.impl.hostsModel.firstHost : "");
                });
            }
            else // Cloud system
            {
                currentAreaItem.userName = Qt.binding( function() { return control.ownerDescription; });
                currentAreaItem.enabled = Qt.binding( function()
                {
                    return control.impl.isOnline && control.isAvailable;
                });
            }
        }
    }

    Connections
    {
        target: (areaLoader.item ? areaLoader.item : null);
        ignoreUnknownSignals: true;

        onConnectRequested: { control.impl.connectToLocalSystem(); }
    }

    Connections
    {
        target: context;
        onResetAutoLogin:
        {
            if (impl.tileType === control.impl.kLocalSystemTileType)
                control.areaLoader.item.autoLogin = false;
        }
    }

    property QtObject impl: QtObject
    {
        property var hostsModel: QnSystemHostsModel { systemId: control.systemId; }
        property var recentConnectionsModel: QnRecentLocalConnectionsModel { systemName: control.systemName; }

        // TODO: add enum to c++ code, add type info to model
        readonly property int kFactorySystemTileType: 0;
        readonly property int kCloudSystemTileType: 1;
        readonly property int kLocalSystemTileType: 2;

        property int tileType:
        {
            if (control.isFactoryTile)
                return kFactorySystemTileType;
            else if (control.isCloudTile)
                return kCloudSystemTileType;
            else
                return kLocalSystemTileType;
        }

        readonly property bool isOnline: !control.impl.hostsModel.isEmpty
        readonly property color standardColor: Style.colors.custom.systemTile.background
        readonly property color hoveredColor: Style.lighterColor(standardColor)
        readonly property color inactiveColor: Style.colors.shadow

        readonly property bool hasSavedConnection:
        {
            if (control.isFactoryTile || control.isCloudTile)
                return false;

            var systemTile = areaLoader.item;
            if (!systemTile)
                return false;

            return systemTile.savePassword && systemTile.selectedUser.length &&
                systemTile.selectedPassword.length;
        }

        function connectToLocalSystem()
        {
            if (isFactoryTile || isCloudTile)
                return;

            var tile  = control.areaLoader.item;
            console.log("Connecting to local system<", control.systemName,
                "host <", tile.selectedHost, "> with credentials: ",
                tile.selectedUser, ":", tile.selectedPassword,
                tile.savePassword, tile.autoLogin);

            context.connectToLocalSystem(
                control.systemId, tile.selectedHost,
                tile.selectedUser, tile.selectedPassword,
                tile.savePassword, tile.autoLogin);
        }
    }
}
