import QtQuick 2.6;
import Nx.Models 1.0;

import "."

BaseTile
{
    id: control;

    property string systemName;
    property string ownerDescription;

    property bool factorySystem: false;
    property bool isCompatibleInternal: false;
    property bool safeMode: false;
    property bool isFactoryTile: impl.isFactoryTile;

    property bool isRunning: false;
    property bool isReachable: false;

    property string wrongVersion;
    property string compatibleVersion;

    // TODO: #ynikitenkov Will be available in 3.1, remove property and related code.
    readonly property bool offlineCloudConnectionsDisabled: true;

    onSystemIdChanged: { forceCollapsedState(); }

    isConnecting: ((control.systemId == context.connectingToSystem)
        && context.connectingToSystem.length && !impl.isFactoryTile);

    isAvailable:
    {
        if (impl.isFactoryTile)
            return true;

        if (wrongVersion.length || !isCompatibleInternal)
            return false;


        // TODO: #ynikitenkov remove condition below in 3.1
        if (offlineCloudConnectionsDisabled && isCloudTile && !context.isCloudEnabled)
            return false;

        return control.isConnectable;
    }

    tileColor:
    {
        if (!control.isAvailable)
            return Style.colors.custom.systemTile.disabled;

        if (control.isHovered)
        {
            return (impl.isFactoryTile ? Style.colors.custom.systemTile.factorySystemHovered
                : Style.colors.custom.systemTile.backgroundHovered);
        }

        return (impl.isFactoryTile ? Style.colors.custom.systemTile.factorySystemBkg
                : Style.colors.custom.systemTile.background);
    }

    secondaryIndicator
    {
        visible: control.safeMode;
        text: qsTr("SAFE MODE");
        textColor: Style.colors.shadow;
        color: Style.colors.yellow_main;
    }

    indicator
    {
        visible:
        {
            if (control.impl.isFactoryTile)
                return false;    //< We don't have indicator for new systems

            return (wrongVersion.length || compatibleVersion.length
                || !control.isConnectable || !isCompatibleInternal);
        }

        text:
        {
            if (!isCompatibleInternal)
                return qsTr("INCOMPATIBLE");
            if (wrongVersion.length)
                return wrongVersion;
            if (compatibleVersion.length)
                return compatibleVersion;
            if (!control.isRunning)
                return qsTr("OFFLINE");
            if (!control.isReachable)
                return qsTr("UNREACHABLE");

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

    NxPopupMenu
    {
        id: tileMenu;
        NxMenuItem
        {
            text: "Edit";
            leftPadding: 16;
            rightPadding: 16;

            onTriggered: control.toggle();
        }
    }

    onCollapsedTileClicked:
    {
        if (!control.isAvailable)
            return;

        if (buttons == Qt.RightButton)
        {
            if (control.menuButton.visible)
            {
                tileMenu.x = x;
                tileMenu.y = y;
                tileMenu.open();
            }
            return;
        }

        switch(control.impl.tileType)
        {
            case control.impl.kFactorySystemTileType:
                var factorySystemHost = areaLoader.item.host;
                control.impl.connectToLocalSystem();
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
        ? qsTr("New Server") : systemName);

    menuButton
    {
        visible: impl.hasSavedConnection && control.isAvailable;

        menu: tileMenu;
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
                currentAreaItem.isExpandedTile = Qt.binding( function() { return control.isExpanded; });
                currentAreaItem.expandedOpacity = Qt.binding( function() { return control.expandedOpacity; });
                currentAreaItem.hostsModel = control.impl.hostsModel;
                currentAreaItem.authenticationDataModel = control.impl.authenticationDataModel;
                currentAreaItem.enabled = Qt.binding( function () { return control.isAvailable; });
                currentAreaItem.prevTabObject = Qt.binding( function() { return control.collapseButton; });
                currentAreaItem.isConnecting = Qt.binding( function() { return control.isConnecting; });
                currentAreaItem.factorySystem = Qt.binding( function() { return control.factorySystem; });
                currentAreaItem.localId = Qt.binding( function() { return control.localId; });
            }
            else if (control.impl.tileType === control.impl.kFactorySystemTileType)
            {
                currentAreaItem.host = control.impl.hostsModelAccessor.getData("url", 0) || "";
                currentAreaItem.displayHost =
                    control.impl.hostsModelAccessor.getData("display", 0) || "";
            }
            else // Cloud system
            {
                currentAreaItem.userName = Qt.binding( function() { return control.ownerDescription; });
                currentAreaItem.isConnectable = Qt.binding( function() { return control.isConnectable; });
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
        property var hostsModel: SystemHostsModel { systemId: control.systemId; }
        property var hostsModelAccessor: ModelDataAccessor { model: control.impl.hostsModel; }
        property var authenticationDataModel: AuthenticationDataModel
        {
            systemId: control.localId;
        }

        // TODO: add enum to c++ code, add type info to model
        readonly property int kFactorySystemTileType: 0;
        readonly property int kCloudSystemTileType: 1;
        readonly property int kLocalSystemTileType: 2;

        property bool isFactoryTile: control.factorySystem && !control.safeMode;

        property int tileType:
        {
            if (isFactoryTile)
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
            if (isFactoryTile || control.isCloudTile)
                return false;

            var systemTile = areaLoader.item;
            if (!systemTile)
                return false;

            return systemTile.savePassword && systemTile.selectedUser.length &&
                systemTile.selectedPassword.length;
        }

        function connectToLocalSystem()
        {
            if (isCloudTile)
                return;

            var tile  = control.areaLoader.item;
            if (isFactoryTile)
            {
                var factorySystemHost = areaLoader.item.host;
                console.log("Trying tp setup factory system <", control.systemName,
                    ">, host <", factorySystemHost, ">");

                /**
                  * Discussed with R. Vasilenko - we can rely on admin/admin
                  * credentials for factory (new) systems. Otherwise it is error
                  * situation  on server side
                  * TODO: ynikitenkov use helpers::kFactorySystem... from network/system_helpers.h
                  */
                var kFactorySystemUser = "admin";
                var kFactorySystemPassword = "admin";
                context.connectToLocalSystem(
                    control.systemId, factorySystemHost,
                    kFactorySystemUser, kFactorySystemPassword,
                    false, false);
            }
            else
            {
                console.log("Connecting to local system <", control.systemName,
                    ">, host <", tile.selectedHost, ">, user <", tile.selectedUser, ">");

                context.connectToLocalSystem(
                    control.systemId, tile.selectedHost,
                    tile.selectedUser, tile.selectedPassword,
                    tile.savePassword, tile.autoLogin);
            }



        }
    }
}
