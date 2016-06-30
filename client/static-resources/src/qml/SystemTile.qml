
import QtQuick 2.6;
import NetworkOptix.Qml 1.0;

import "."

BaseTile
{
    id: control;

    property string systemName;
    property string ownerDescription;

    property var systemId;
    property bool isFactoryTile: false;
    property bool isCloudTile: false;

    property string wrongVersion;
    property string wrongCustomization;
    property string compatibleVersion;

    isAvailable:
    {
        if (isFactoryTile)
            return true;

        if (wrongCustomization.length || wrongVersion.length)
            return false;

        if (isCloudTile)
            return control.areaLoader.item && control.areaLoader.item.isOnline;

        return true;
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
        else if (control.isHovered)
            return Style.colors.custom.systemTile.backgroundHovered;
        else if (control.isAvailable)
            return Style.colors.custom.systemTile.background;
    }

    indicator
    {
        visible: ((impl.tileType !== impl.kFactorySystemTileType) &&
            (wrongVersion.length || wrongCustomization.length ||
             compatibleVersion.length || !impl.isOnline));

        text:
        {
            if (wrongCustomization.length)
                return wrongCustomization;
            else if (wrongVersion.length)
                return wrongVersion;
            else if (compatibleVersion.length)
                return compatibleVersion;
            else if (!impl.isOnline)
                return "OFFLINE";

            return "";
        }

        textColor:
        {
           if (wrongCustomization.length || wrongVersion.length ||
                compatibleVersion.length)
           {
               return Style.colors.shadow;
           }
           else
               return Style.colors.windowText;
        }

        color:
        {
            if (wrongCustomization.length || wrongVersion.length)
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
            console.log("Show wizard for system <", systemName
                , ">, host <", factorySystemHost, ">");
            context.setupFactorySystem(factorySystemHost);
        }
        else if (isCloudTile)
        {
            var cloudHost = control.impl.hostsModel.firstHost;
            console.log("Connecting to cloud system <", systemName
                , ">, throug the host <", cloudHost, ">");
            context.connectToCloudSystem(cloudHost);
        }
        else // Local system tile
        {
            if (impl.hasSavedConnection)
                control.impl.connectToLocalSystem();
            else
                toggle();
        }
    }

    titleLabel.text: (isFactoryTile ? "New System" : systemName);

    menuButton
    {
        visible: impl.hasSavedConnection;

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
                currentAreaItem.isExpandedTile = Qt.binding( function() { return control.isExpanded; });
                currentAreaItem.expandedOpacity = Qt.binding( function() { return control.expandedOpacity; });
                currentAreaItem.hostsModel = control.impl.hostsModel;
                currentAreaItem.recentUserConnectionsModel = control.impl.recentConnectionsModel;
                currentAreaItem.enabled = Qt.binding( function () { return control.isAvailable; });
                currentAreaItem.prevTabObject = Qt.binding( function() { return control.collapseButton; });
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
                currentAreaItem.isOnline = Qt.binding( function() { return control.impl.isOnline; } )
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
        property var recentConnectionsModel: QnRecentUserConnectionsModel { systemName: control.systemName; }

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

        readonly property bool isOnline: !control.impl.hostsModel.isEmpty;
        readonly property color standardColor: Style.colors.custom.systemTile.background;
        readonly property color hoveredColor: Style.lighterColor(standardColor);
        readonly property color inactiveColor: Style.colors.shadow;

        readonly property bool hasSavedConnection:
        {
            if (control.isFactoryTile || control.isCloudTile)
                return false;

            var systemTile = areaLoader.item;
            return systemTile.savePassword && systemTile.selectedUser.length &&
                systemTile.selectedPassword.length;
        }

        function connectToLocalSystem()
        {
            if (isFactoryTile || isCloudTile)
                return;

            var tile  = control.areaLoader.item;
            console.log("Connecting to local system<", control.systemName
                , "host <", tile.selectedHost, "> with credentials: "
                , tile.selectedUser, ":", tile.selectedPassword
                , tile.savePassword, tile.autoLogin);

            context.connectToLocalSystem(tile.selectedHost,
                tile.selectedUser, tile.selectedPassword,
                tile.savePassword, tile.autoLogin);
        }
    }
}
