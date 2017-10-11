import QtQuick 2.6;
import Nx 1.0;
import Nx.Models 1.0;
import Nx.WelcomeScreen 1.0;

BaseTile
{
    id: control;

    property string systemName;
    property string ownerDescription;

    property bool factorySystem: false;
    property bool isCompatible: false;
    property bool safeMode: false;
    property bool isFactoryTile: impl.isFactoryTile;

    property bool isRunning: false;
    property bool isReachable: false;

    property string wrongVersion;
    property string compatibleVersion;

    isConnecting: ((control.systemId == context.connectingToSystem)
        && context.connectingToSystem.length && !impl.isFactoryTile);

    isAvailable:
    {
        if (impl.isFactoryTile)
            return true;

        if (wrongVersion || !isCompatible)
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

    property QtObject primaryIndicatorProperties: QtObject
    {
        property bool visible: control.wrongVersion || control.compatibleVersion
            || !control.isConnectable || !isCompatible

        property string text:
        {
            if (!isCompatible)
                return qsTr("INCOMPATIBLE");
            if (wrongVersion)
                return wrongVersion.toString(SoftwareVersion.BugfixFormat);
            if (compatibleVersion)
                return compatibleVersion.toString(SoftwareVersion.BugfixFormat);
            if (!control.isRunning)
                return qsTr("OFFLINE");
            if (!control.isReachable)
                return qsTr("UNREACHABLE");

            return "";
        }

        property color textColor:
        {
           if (wrongVersion || compatibleVersion || !isCompatible)
               return Style.colors.shadow;
           else
               return Style.colors.windowText;
        }

        property color color:
        {
            if (wrongVersion || !isCompatible)
                return Style.colors.red_main;
            else if (compatibleVersion)
                return Style.colors.yellow_main;
            else
                return Style.colors.custom.systemTile.offlineIndicatorBkg;
        }
    }

    property QtObject secondaryIndicatorProperties: QtObject
    {
        property bool visible: control.safeMode;
        property string text: qsTr("SAFE MODE");
        property color textColor: Style.colors.shadow;
        property color color: Style.colors.yellow_main;
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
                var cloudHost = control.impl.hostsModelAccessor.getData(0, "url") || "";
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

        function setupIndicator(indicator, properties)
        {
            indicator.visible = Qt.binding(function() { return properties.visible })
            indicator.color = Qt.binding(function() { return properties.color })
            indicator.text = Qt.binding(function() { return properties.text })
            indicator.textColor = Qt.binding(function() { return properties.textColor })
        }

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
                currentAreaItem.isAvailable = Qt.binding( function () { return control.isAvailable; });
                currentAreaItem.prevTabObject = Qt.binding( function() { return control.collapseButton; });
                currentAreaItem.isConnecting = Qt.binding( function() { return control.isConnecting; });
                currentAreaItem.factorySystem = Qt.binding( function() { return control.factorySystem; });
                currentAreaItem.localId = Qt.binding( function() { return control.localId; });
            }
            else if (control.impl.tileType === control.impl.kFactorySystemTileType)
            {
                currentAreaItem.host = control.impl.hostsModelAccessor.getData(0, "url") || "";
                currentAreaItem.displayHost =
                    control.impl.hostsModelAccessor.getData(0, "display") || "";
            }
            else // Cloud system
            {
                currentAreaItem.userName = Qt.binding( function() { return control.ownerDescription; });
                currentAreaItem.isConnectable = Qt.binding( function() { return control.isConnectable; });
                currentAreaItem.isAvailable = Qt.binding( function() { return control.isAvailable; });
            }

            var indicators = currentAreaItem.indicators
            indicators.opacity = Qt.binding(function() { return control.collapsedItemsOpacity })
            setupIndicator(indicators.primaryIndicator, control.primaryIndicatorProperties)
            setupIndicator(indicators.secondaryIndicator, control.secondaryIndicatorProperties)
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
        property var hostsModel: SystemHostsModel
        {
            systemId: control.systemId;
            localSystemId: control.localId;
        }

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
                console.log("Trying to setup factory system <", control.systemName,
                    ">, host <", factorySystemHost, ">");

                /**
                  * Discussed with R. Vasilenko - we can rely on admin/admin
                  * credentials for factory (new) systems. Otherwise it is error
                  * situation  on server side
                  * TODO: #ynikitenkov use helpers::kFactorySystem... from network/system_helpers.h
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
