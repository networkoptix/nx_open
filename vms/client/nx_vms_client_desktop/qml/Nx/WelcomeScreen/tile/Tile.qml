// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4
import Qt5Compat.GraphicalEffects

import Nx.Core 1.0
import Nx.Models 1.0
import Nx.Controls 1.0 as Nx
import Nx.Core.Controls 1.0
import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Button
{
    id: tile

    property int gridCellWidth: 0
    property int gridCellHeight: 0

    property Item openedParent: null
    property Item _initialParent: null

    readonly property string title: model.systemName
    readonly property string localId: model.localId

    // Actually that's a tile id, calculated in the system finders.
    readonly property string systemId: model.systemId
    readonly property string owner: model.ownerDescription
    readonly property int visibilityScope: model.visibilityScope

    readonly property bool online: model.isOnline
    readonly property bool cloud: model.isCloudSystem
    readonly property bool unreachable: !model.isReachable
    readonly property bool incompatible: !model.isCompatibleToDesktopClient
    readonly property bool isFactorySystem: model.isFactorySystem
    readonly property bool is2FaEnabledForSystem: model.is2FaEnabledForSystem
    readonly property bool systemRequires2FaEnabledForUser:
        model.isCloudSystem && model.is2FaEnabledForSystem  && !context.is2FaEnabledForUser

    readonly property bool saasSuspended: model.isSaasSuspended
    readonly property bool saasShutDown: model.isSaasShutDown

    property var visibilityMenuModel: null
    property bool hideActionEnabled: false
    property bool shiftPressed: false

    readonly property bool connectable: online && !incompatible && !unreachable
    readonly property bool loggedIn: logicImpl.hasSavedConnection

    property string host: ""
    property string serverId: ""

    property bool isConnecting: false
    property bool isExpanded: false

    signal startExpanding()
    signal shrinked()
    signal clickedToConnect()
    signal releaseFocus()

    function closeTileMenu()
    {
        closedTileItem.closeMenu()
    }

    function forgetSavedPassword()
    {
        context.forgetUserPassword(tile.localId, logicImpl.user)
    }

    function connect()
    {
        clickedToConnect()

        tile.isConnecting = true

        if (tile.cloud)
            context.connectToCloudSystem(tile.systemId)
        else
            logicImpl.connectToLocalSystem()

        if (!logicImpl.savePassword && logicImpl.isPasswordSaved)
            forgetSavedPassword()
    }

    function shrinkAndStopConnecting()
    {
        if (isExpanded)
            shrink()
        tile.isConnecting = false
    }

    function deleteSystem()
    {
        context.deleteSystem(systemId, localId)
    }

    function forgetAllCredentials()
    {
        if (openedTileLoader.item)
        {
            openedTileLoader.item.password = ""
            openedTileLoader.item.savePassword = false
        }

        context.forgetAllCredentials(tile.localId)
    }

    function expand()
    {
        startExpanding()
        openedTileLoader.load()
        closeTileMenu()
        state = "expanded"
        focus = true
    }

    function shrink()
    {
        if (state == "compact")
            return

        state = "compact"
        releaseFocus()
        shrinked()
    }

    function setErrorMessage(errorMessage, isLoginError)
    {
        if (!isConnecting)
            return

        isConnecting = false
        if (openedTileLoader.item)
            openedTileLoader.item.setError(errorMessage, isLoginError)
    }

    function loadOpenTileItem()
    {
        if (!openedTileLoader.sourceComponent)
            openedTileLoader.sourceComponent = openedTileComponent
    }

    z: 1
    width: gridCellWidth
    height: gridCellHeight

    clip: true
    padding: 0
    leftPadding: 0
    rightPadding: 0

    focusPolicy: Qt.NoFocus

    state: "compact"

    Component.onCompleted: _initialParent = parent

    Component.onDestruction: shrink()

    Keys.onPressed: function(event)
    {
        if (!isExpanded)
            return

        if (event.key === Qt.Key_Escape)
            shrink()
        else
            event.accepted = true
    }

    onCloudChanged:
    {
        if (isExpanded)
            shrink()
    }

    onConnectableChanged:
    {
        if (connectable)
            return

        if (isExpanded)
        {
            shrink()
            context.showSystemWentToOfflineNotification()
        }
        else if (isConnecting)
        {
            context.showSystemWentToOfflineNotification()
        }

        isConnecting = false
    }

    onClicked:
    {
        if (!online && host)
            context.testSystemOnline(host)

        if (isExpanded || !online || unreachable || incompatible)
            return

        if (!cloud && !logicImpl.isFactoryTile && !logicImpl.hasSavedConnection)
        {
            expand()
            return
        }

        openedTileLoader.load()
        connect()
    }

    contentItem: Item
    {
        Item
        {
            anchors.fill: parent

            opacity: !tile.isExpanded && tile.isConnecting ? 0.4 : 1

            Image
            {
                id: stateIcon

                x: 16
                anchors.verticalCenter: titleRect.verticalCenter
                width: 20
                height: 20

                fillMode: Image.Stretch

                source: visibilityScope === WelcomeScreen.FavoriteTileVisibilityScope
                    ? "image://svg/skin/welcome_screen/tile/star_full.svg"
                    : "image://svg/skin/welcome_screen/tile/eye_full.svg"
                sourceSize: Qt.size(width, height)

                visible: visibilityScope !== WelcomeScreen.DefaultTileVisibilityScope
            }

            Item
            {
                id: titleRect

                x: 16 + (stateIcon.visible ? stateIcon.width + 4 : 0)
                y: 11
                width: tile.width - x - 40
                height: 23

                // Label.elide doesn't work for RichText.
                clip: true

                Label
                {
                    height: titleRect.height
                    width: titleRect.width

                    elide: Text.ElideRight
                    font.pixelSize: 20
                    color: tile.connectable ? ColorTheme.colors.light4 : ColorTheme.colors.dark14

                    textFormat: context.richTextEnabled ? Text.RichText : Text.PlainText
                    text: tile.title

                    GlobalToolTip.text: tile.title
                }
            }

            ClosedTileItem
            {
                id: closedTileItem

                width: tile.width
                height: tile.height
                visible: opacity > 0
                tile: tile
                z: 2

                addressText: tile.cloud ? tile.owner : tile.host
            }

            Loader
            {
                id: openedTileLoader

                width: childrenRect.width
                height: childrenRect.height

                visible: opacity > 0

                function load()
                {
                    if (!sourceComponent)
                        sourceComponent = openedTileComponent
                }

                Component
                {
                    id: openedTileComponent

                    OpenedTileItem
                    {
                        width: tile.width

                        systemHostsModel: tile.logicImpl.systemHostsModel
                        authenticationDataModel: tile.logicImpl.authenticationDataModel

                        isConnecting: tile.isConnecting

                        onConnect: tile.connect()
                        onShrink: tile.shrink()
                        onSavedPasswordCleared: tile.forgetSavedPassword()
                        onSavePasswordChanged: logicImpl.savePassword = savePassword
                        onHostTextChanged: logicImpl.hostDataAccessor.customHostText = hostText
                        onHostIndexChanged: logicImpl.hostDataAccessor.index = hostIndex
                        onUserChanged: logicImpl.user = user
                        onPasswordChanged: logicImpl.newPassword = password

                        onAbortConnectionProcess:
                        {
                            tile.isConnecting = false
                            context.abortConnectionProcess()
                        }
                    }
                }
            }
        }

        Nx.NxDotPreloader
        {
            id: connectingProgressDots

            z: 2
            anchors.centerIn: parent

            color: ColorTheme.windowText
            visible: !tile.isExpanded && running
            running: tile.isConnecting
        }
    }

    background: Rectangle
    {
        radius: 2

        opacity: !tile.isExpanded && tile.isConnecting ? 0.4 : 1

        color:
        {
            if (!tile.connectable)
                return ColorTheme.colors.dark5

            if (tile.isExpanded)
                return ColorTheme.colors.dark8

            return (tile.hovered && !closedTileItem.menuHovered)
                ? ColorTheme.colors.dark8
                : ColorTheme.colors.dark7
        }
    }

    states:
    [
        State
        {
            name: "expanded"

            ParentChange
            {
                target: tile
                parent: openedParent
                x: Math.floor((openedParent.width - tile.width) / 2)
                y: Math.floor((openedParent.height - tile.height) / 2)
            }

            PropertyChanges
            {
                target: tile
                width: MathUtils.bound(
                    tile.gridCellWidth * 1.2,
                    openedTileLoader.item.requiredWidth,
                    tile.gridCellWidth * 2)
                height: openedTileLoader.y + openedTileLoader.height
            }

            PropertyChanges
            {
                target: closedTileItem
                opacity: 0
            }

            PropertyChanges
            {
                target: openedTileLoader
                opacity: 1
            }

            PropertyChanges
            {
                target: shadow
                opacity: 1
            }
        },

        State
        {
            name: "compact"

            ParentChange
            {
                target: tile
                parent: _initialParent
                x: 0
                y: 0
            }

            PropertyChanges
            {
                target: tile
                width: gridCellWidth
                height: gridCellHeight
            }

            PropertyChanges
            {
                target: closedTileItem
                opacity: 1
            }

            PropertyChanges
            {
                target: openedTileLoader
                opacity: 0
            }

            PropertyChanges
            {
                target: shadow
                opacity: 0
            }
        }
    ]

    transitions: Transition
    {
        SequentialAnimation
        {
            PropertyAction
            {
                target: tile
                properties: "isExpanded"
                value: tile.state === "expanded"
            }

            ParallelAnimation
            {
                ParentAnimation
                {
                    via: tile.openedParent

                    NumberAnimation
                    {
                        properties: "x, y"
                        easing.type: Easing.InOutCubic
                        duration: 400
                    }
                }

                NumberAnimation
                {
                    properties: "width, height"
                    easing.type: Easing.OutCubic
                    duration: 400
                }

                NumberAnimation
                {
                    targets: [closedTileItem, openedTileLoader]
                    properties: "opacity"
                    easing.type: Easing.OutCubic
                    duration: 200
                }
            }
        }

        // Fix tile position when system moved inside grid while tile was shrinking.
        onRunningChanged:
        {
            if (!running &&
                tile.parent == tile._initialParent &&
                tile.state == "compact" &&
                (tile.x != 0 || tile.y != 0))
            {
                tile.x = tile.y = 0
            }
        }
    }

    property QtObject logicImpl: QtObject
    {
        readonly property bool isFactoryTile: tile.isFactorySystem

        property bool savePassword: false
        property string user: ""
        property bool isPasswordSaved: false
        property string newPassword: ""

        readonly property bool hasSavedConnection:
        {
            if (isFactoryTile || tile.cloud)
                return false

            return savePassword && tile.host && user && isPasswordSaved
        }

        property SystemHostsModel systemHostsModel: SystemHostsModel
        {
            systemId: tile.systemId
            localSystemId: tile.localId
            forcedLocalhostConversion: true
        }

        property AuthenticationDataModel authenticationDataModel: AuthenticationDataModel
        {
            localSystemId: tile.localId
        }

        // 1. Inits selected host index value before openedTileComponent loaded.
        // 2. Calculates final host & serverId values.
        property var hostDataAccessor: ModelDataAccessor
        {
            property string customHostText: ""
            property int index: -1

            function tryToInitIndex()
            {
                if (count > 0)
                    index = 0
            }

            function updateOutputProperties()
            {
                tile.host = index >= 0 ? getData(index, "display") : customHostText
                tile.serverId = index >= 0 ? getData(index, "serverId") : ""

                if (index != -1)
                {
                    closedTileItem.versionText =
                        logicImpl.systemHostsModel.getRequiredSystemVersion(index)
                }
            }

            model: logicImpl.systemHostsModel

            Component.onCompleted: tryToInitIndex()

            onCustomHostTextChanged: updateOutputProperties()
            onIndexChanged: updateOutputProperties()
            onDataChanged: (startRow, endRow) =>
            {
                if (startRow == 0)
                    index = 0
                updateOutputProperties()
            }

            onCountChanged:
            {
                if (index == -1)
                    tryToInitIndex()
                else if (count == 0)
                    index = -1

                updateOutputProperties()
            }
        }

        // Inits user, password, savePassword values before openedTileComponent loaded.
        property var authDataAccessor: ModelDataAccessor
        {
            property string localSystemId: ""

            model: logicImpl.authenticationDataModel

            Component.onCompleted: tryToInitUser()

            onCountChanged:
            {
                tryToInitUser()
            }

            function tryToInitUser()
            {

                if (count > 0 && localSystemId !== logicImpl.authenticationDataModel.localSystemId)
                {
                    updateUserAndPassword(0)
                    localSystemId = logicImpl.authenticationDataModel.localSystemId
                }
                else
                {
                    // Fill properties without bindings with the default values to avoid using
                    // data made for the previous system.
                    logicImpl.savePassword = false
                    logicImpl.user = ""
                    logicImpl.isPasswordSaved = false
                    logicImpl.newPassword = ""
                }
            }

            function updateUserAndPassword(currentItemIndex)
            {
                const credentials = getData(currentItemIndex, "credentials")

                logicImpl.user = (credentials && credentials.user)
                logicImpl.isPasswordSaved = context.saveCredentialsAllowed
                    ? (credentials && credentials.isPasswordSaved)
                    : ""
                logicImpl.savePassword = logicImpl.isPasswordSaved && context.saveCredentialsAllowed
            }
        }

        function connectToLocalSystem()
        {
            if (isFactoryTile)
            {
                context.setupFactorySystem(tile.systemId, tile.host, tile.serverId)
            }
            else if (logicImpl.isPasswordSaved && !tile.isExpanded)
            {
                context.connectToLocalSystemUsingSavedPassword(
                    tile.systemId,
                    tile.host,
                    tile.serverId,
                    user,
                    logicImpl.savePassword)
            }
            else // Connecting using opened tile
            {
                if (openedTileLoader.item.isPasswordSaved)
                {
                    context.connectToLocalSystemUsingSavedPassword(
                        tile.systemId,
                        tile.host,
                        tile.serverId,
                        user,
                        logicImpl.savePassword)
                }
                else
                {
                    context.connectToLocalSystem(
                        tile.systemId,
                        tile.host,
                        tile.serverId,
                        user,
                        newPassword,
                        /*storePassword*/ savePassword && context.saveCredentialsAllowed)
                }
            }
        }
    }

    Connections
    {
        target: context

        function onVisibleControlsChanged()
        {
            if (tile.isExpanded)
                tile.shrink()
        }

        function onDropConnectingState()
        {
            tile.isConnecting = false
        }
    }

    DropShadow
    {
        id: shadow

        anchors.fill: background
        z:-1

        visible: opacity > 0
        radius: 24
        color: ColorTheme.shadow
        source: background
    }
}
