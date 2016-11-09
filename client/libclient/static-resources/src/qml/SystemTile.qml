
import QtQuick 2.6;
import NetworkOptix.Qml 1.0;

import "."

BaseTile
{
    id: control;

    property string systemName;
    property string ownerDescription;

    property bool isFactoryTile: false;
    property bool isCompatibleInternal: false;

    property string wrongVersion;
    property string compatibleVersion;

    // TODO: #ynikitenkov Will be available in 3.1, remove property and related code.
    readonly property bool offlineCloudConnectionsDisabled: true;

    onSystemIdChanged: { forceCollapsedState(); }

    isConnecting: ((control.systemId == context.connectingToSystem)
        && context.connectingToSystem.length && !isFactoryTile);

    isAvailable:
    {
        if (isFactoryTile)
            return true;

        if (wrongVersion.length || !isCompatibleInternal)
            return false;


        // TODO: #ynikitenkov remove condition below in 3.1
        if (offlineCloudConnectionsDisabled && isCloudTile && !context.isCloudEnabled)
            return false;

        return control.isOnline;
    }

    tileColor:
    {
        if (!control.isAvailable)
            return Style.colors.custom.systemTile.disabled;

        if (control.isHovered)
        {
            return (isFactoryTile ? Style.colors.custom.systemTile.factorySystemHovered
                : Style.colors.custom.systemTile.backgroundHovered);
        }

        return (isFactoryTile ? Style.colors.custom.systemTile.factorySystemBkg
                : Style.colors.custom.systemTile.background);
    }

    indicator
    {
        visible:
        {
            if (control.isFactoryTile)
                return false;    //< We don't have indicator for new systems

            return (wrongVersion.length || compatibleVersion.length
                || !control.isOnline || !isCompatibleInternal);
        }

        text:
        {
            if (!isCompatibleInternal)
                return qsTr("INCOMPATIBLE");
            if (wrongVersion.length)
                return wrongVersion;
            if (compatibleVersion.length)
                return compatibleVersion;
            if (!control.isOnline)
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

        switch(control.impl.tileType)
        {
            case control.impl.kFactorySystemTileType:
                var factorySystemHost = areaLoader.item.host;
                console.log("Show wizard for system <", systemName,
                    ">, host <", factorySystemHost, ">");
                context.setupFactorySystem(factorySystemHost);
                break;

            case control.impl.kCloudSystemTileType:
                var cloudHost = control.impl.hostsModel.firstHost;
                console.log("Connecting to cloud system <", systemName,
                    ">, through the host <", cloudHost, ">");
                context.connectToCloudSystem(control.systemId, cloudHost);
                break;

            case control.impl.kLocalSystemTileType:
                if (impl.hasSavedConnection)
                    control.impl.connectToLocalSystem();
                else
                    toggle();

                break;

            default:
                console.error("Unknown tile type: ", control.impl.tileType);
                break;
        }
    }

    titleLabel.text: (control.impl.tileType == control.impl.kFactorySystemTileType
        ? qsTr("New System") : systemName);

    menuButton
    {
        visible: impl.hasSavedConnection && control.isAvailable;

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
        switch(control.impl.tileType)
        {
            case control.impl.kFactorySystemTileType:
                return "tile_areas/FactorySystemTileArea.qml";
            case control.impl.kCloudSystemTileType:
                return "tile_areas/CloudSystemTileArea.qml";
            case control.impl.kLocalSystemTileType:
                return "tile_areas/LocalSystemTileArea.qml";
            default:
                console.error("Unknown tile type: ", control.impl.tileType);
                break;
        }
        return "";
    }

    Connections
    {
        target: areaLoader;

        onStatusChanged:
        {
            if (areaLoader.status != Loader.Ready)
                return;

            var currentAreaItem = control.areaLoader.item;
            if (!currentAreaItem)
                return;

            if (control.impl.tileType === control.impl.kLocalSystemTileType)
            {
                currentAreaItem.isOnline = Qt.binding( function() { return control.isOnline; });
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
                        control.impl.hostsModel.getData("url", 0): "");
                });
                currentAreaItem.displayHost = Qt.binding( function()
                {
                    return (control.impl.hostsModel ?
                        control.impl.hostsModel.getData("display", 0): "");
                });
            }
            else // Cloud system
            {
                currentAreaItem.userName = Qt.binding( function() { return control.ownerDescription; });
                currentAreaItem.isOnline = Qt.binding( function() { return control.isOnline; });
                currentAreaItem.enabled = Qt.binding( function() { return control.isAvailable; });
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
        property var recentConnectionsModel: QnRecentLocalConnectionsModel { systemId: control.localId; }

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

        readonly property color standardColor: Style.colors.custom.systemTile.background;
        readonly property color hoveredColor: Style.lighterColor(standardColor);
        readonly property color inactiveColor: Style.colors.shadow;

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
