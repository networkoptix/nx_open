// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml 2.11
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Window 2.11

import Nx 1.0
import Nx.Core 1.0
import Nx.Models 1.0
import Nx.Controls 1.0
import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0
import Nx.Debug 1.0

Rectangle
{
    id: welcomeScreen

    readonly property int sideBordersWidth: width * 0.02 + 24

    readonly property bool hasHiddenSystems: systemModel
        ? systemModel.systemsCount !== systemModel.totalSystemsCount
        : false

    readonly property bool complexVisibilityMode: systemModel
        && ((systemModel.totalSystemsCount > context.simpleModeTilesNumber)
        || hasHiddenSystems
        || (systemModel.visibilityFilter !== WelcomeScreen.AllSystemsTileScopeFilter))
        || searchEdit.text
        || searchEdit.query

    property ConnectTilesModel systemModel: context.visibleControls && context.gridEnabled
        ? context.systemModel
        : null

    function loginToCloud()
    {
        context.loginToCloud()
    }

    function onlyShiftModifierPressed(event)
    {
        return !!((event.modifiers & Qt.ShiftModifier)
            && !(event.modifiers & Qt.ControlModifier))
    }

    visible: context.visibleControls

    color: ColorTheme.colors.dark3

    focus: true

    onSystemModelChanged: globalInterfaceLock.enabled = false

    onWidthChanged: grid.updateMaxVisibleRowCountFromScratch()
    onHeightChanged: grid.updateMaxVisibleRowCountFromScratch()
    onComplexVisibilityModeChanged: grid.updateMaxVisibleRowCountFromScratch()

    Keys.onPressed: (event) => grid.shiftPressed = onlyShiftModifierPressed(event)
    Keys.onReleased: (event) => grid.shiftPressed = onlyShiftModifierPressed(event)

    Shortcut
    {
        sequence: "Ctrl+F"
        enabled: welcomeScreen.Window.visibility !== Window.Hidden
        onActivated: searchEdit.forceActiveFocus()
    }

    Shortcut
    {
        sequence: "Ctrl+Shift+L"
        onActivated: welcomeScreen.loginToCloud()
    }

    Shortcut
    {
        sequence: "Alt+A"
        enabled: systemModel
        onActivated: systemModel.visibilityFilter = WelcomeScreen.AllSystemsTileScopeFilter
    }

    Shortcut
    {
        sequence: "Alt+F"
        enabled: systemModel
        onActivated: systemModel.visibilityFilter = WelcomeScreen.FavoritesTileScopeFilter
    }

    Shortcut
    {
        sequence: "Alt+H"
        enabled: systemModel
        onActivated: systemModel.visibilityFilter = WelcomeScreen.HiddenTileScopeFilter
    }

    MouseArea
    {
        anchors.fill: parent
        z: -1

        onPressed:
        {
            if (pressed)
                focus = true
        }
    }

    Item
    {
        id: header

        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * welcomeScreen.sideBordersWidth
        height: parent.height - grid.height - footer.height + grid.tileSpacing

        Item
        {
            id: logoArea

            readonly property int minHeight: 114
            readonly property int maxHeight: 228

            anchors.horizontalCenter: parent.horizontalCenter
            y: welcomeScreen.complexVisibilityMode
               ? (searchArea.y - height) / 2
               : (header.height + grid.firstRowYShift - height) / 2
            width: logo.width
            height: MathUtils.bound(minHeight, searchArea.y, maxHeight)

            Image
            {
                id: logo

                readonly property int minHeight: 60
                readonly property int maxHeight: 144
                readonly property real squeezefactor:
                    (logoArea.maxHeight - logoArea.minHeight) / (maxHeight - minHeight)

                anchors.centerIn: parent
                width: sourceSize.width
                height: maxHeight - (logoArea.maxHeight - logoArea.height) / squeezefactor

                source: "qrc:/skin/welcome_page/logo.png"
                fillMode: (sourceSize.height < height) && (sourceSize.width < width)
                    ? Image.Pad
                    : Image.PreserveAspectFit
            }
        }

        Informer
        {
            anchors.horizontalCenter: parent.horizontalCenter
            // It's in the middle between y = 0 and visible logo top border.
            y: (logoArea.y + logo.y - height) / 2 + Math.max(0, logo.height - logo.sourceSize.height) / 4

            visible: context.hasCloudConnectionIssue

            text: qsTr("You do not have access to the %1. Please check your internet connection.")
                .arg(Branding.cloudName())
        }

        Item
        {
            id: searchArea

            anchors.bottom: parent.bottom
            anchors.bottomMargin: grid.tileSpacing
            width: parent.width
            height: 28

            ConnectButton
            {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2

                visible: welcomeScreen.complexVisibilityMode

                onClicked: context.connectToAnotherSystem()
            }

            Item
            {
                anchors.horizontalCenter: parent.horizontalCenter
                width: grid.maxColCount > 3
                    ? grid.tileWidth * 2 + grid.tileSpacing
                    : grid.tileWidth
                height: parent.height

                SearchEdit
                {
                    id: searchEdit

                    anchors.fill: parent

                    visible: welcomeScreen.complexVisibilityMode

                    delay: 200

                    onQueryChanged:
                    {
                        if (systemModel)
                            systemModel.setFilterWildcard(query)
                    }
                }

                Keys.onPressed: (event) => event.accepted = true
            }

            VisibilityButton
            {
                id: visibilityButton

                property VisibilityMenu menu: VisibilityMenu
                {
                    model: systemModel

                    onClosed: welcomeScreen.focus = true
                }

                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2

                text: systemModel ? menu.filterToString(systemModel.visibilityFilter) : ""

                visible: welcomeScreen.complexVisibilityMode && !searchEdit.query

                onClicked:
                {
                    menu.width = Math.max(menu.implicitWidth, width)
                    menu.x = width - menu.width
                    menu.y = y + height
                    menu.open()
                }
            }
        }
    }

    PerformanceText
    {
        anchors.top: parent.top
        anchors.right: parent.right
    }

    TileGrid
    {
        id: grid

        // For saving rows count if the only one changed thing is number of systems.
        property int prevMaxVisibleRowCount: 0
        property int maxRowCount: systemModel ? Math.ceil(systemModel.systemsCount / maxColCount) : 0

        function calculateVisibleMaxRowsCount(height)
        {
            return Math.floor(height / (tileHeight + tileSpacing))
        }

        // Do not decrease rows number, for saving max grid height for all visibility modes.
        function updateMaxVisibleRowCount()
        {
            let freeHeight = welcomeScreen.height - footer.height
            if (welcomeScreen.complexVisibilityMode)
                freeHeight -= searchArea.height
            let newMaxVisibleRowCount = grid.calculateVisibleMaxRowsCount(freeHeight - logoArea.maxHeight)
            if (newMaxVisibleRowCount <= 0 || newMaxVisibleRowCount < maxRowCount)
                newMaxVisibleRowCount = grid.calculateVisibleMaxRowsCount(freeHeight - logoArea.minHeight)

            if (newMaxVisibleRowCount > prevMaxVisibleRowCount)
                prevMaxVisibleRowCount = newMaxVisibleRowCount

            maxVisibleRowCount = prevMaxVisibleRowCount
        }

        // Drops max grid height for resizing grid from scratch if grid layout geometry changed.
        function updateMaxVisibleRowCountFromScratch()
        {
            grid.prevMaxVisibleRowCount = 0
            grid.updateMaxVisibleRowCount()
        }

        onMaxRowCountChanged: updateMaxVisibleRowCount()

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: header.bottom
        width: parent.width - 2 * welcomeScreen.sideBordersWidth
        height: maxVisibleRowCount * (tileHeight + tileSpacing) - tileSpacing

        openedTileParent: parent
        model: systemModel

        alignToCenter: !welcomeScreen.complexVisibilityMode
        hideActionEnabled: welcomeScreen.complexVisibilityMode
        onTileClicked: context.setGlobalPreloaderEnabled(false)
        onLockInterface: locked => { globalInterfaceLock.enabled = locked }
        onSetOpenedTileModalityInterfaceLock:
            enabled =>
            {
                openedTileModalityInterfaceLock.enabled = enabled
            }

        TileGridPlaceholder
        {
            anchors.centerIn: parent

            mode:
            {
                if (!systemModel || systemModel.systemsCount > 0)
                    return TileGridPlaceholder.Mode.Disabled

                if (searchEdit.query)
                    return TileGridPlaceholder.Mode.NoSystemsFound
                if (systemModel.visibilityFilter === WelcomeScreen.FavoritesTileScopeFilter)
                    return TileGridPlaceholder.Mode.NoFavorites
                if (systemModel.visibilityFilter === WelcomeScreen.HiddenTileScopeFilter)
                    return TileGridPlaceholder.Mode.NoHidden

                return TileGridPlaceholder.Mode.Disabled
            }
        }
    }

    Rectangle
    {
        id: messageHolder

        anchors.centerIn: parent
        width: messageLabel.implicitWidth + 24 * 2
        height: messageLabel.implicitHeight + 10 * 2
        radius: 2

        visible: context.message
        opacity: visible ? 1.0 : 0.0
        color: ColorTheme.transparent(ColorTheme.colors.brand_d2, 0.8)

        Behavior on opacity { NumberAnimation { duration: 200 } }

        Label
        {
            id: messageLabel

            anchors.centerIn: parent

            font.pixelSize: 22
            color: ColorTheme.colors.brand_contrast

            text: context.message
        }
    }

    Item
    {
        id: footer

        anchors.bottom: parent.bottom
        width: parent.width
        height: 80

        Item
        {
            anchors.top: parent.top
            anchors.topMargin: 32
            x: grid.x
            width: grid.width

            Label
            {
                font.pixelSize: FontConfig.normal.pixelSize

                color: ColorTheme.colors.dark17

                text: BuildInfo.vmsVersion()
            }

            Row
            {
                anchors.right: parent.right

                spacing: 38

                LinkButton
                {
                    font.pixelSize: FontConfig.normal.pixelSize
                    font.underline: true

                    text: qsTr("Official Website")

                    onClicked: Qt.openUrlExternally(Branding.companyUrl())
                }

                LinkButton
                {
                    font.pixelSize: FontConfig.normal.pixelSize
                    font.underline: true

                    text: qsTr("Help & User Manual")

                    onClicked: context.openHelp()
                }

                LinkButton
                {
                    font.pixelSize: FontConfig.normal.pixelSize
                    font.underline: true

                    text: qsTr("Support")

                    onClicked:
                    {
                        const supportAddress = Branding.supportAddress()

                        if (context.checkUrlIsValid(supportAddress))
                        {
                            Qt.openUrlExternally(supportAddress)
                        }
                        else if (supportAddress.includes("@"))
                        {
                            Qt.openUrlExternally("mailto:?to=" + supportAddress)
                        }
                        else
                        {
                            MessageBox.exec(MessageBox.Icon.Information,
                                supportAddress,
                                "",
                                MessageBox.Ok);
                        }
                    }
                }
            }
        }
    }

    GlobalLoaderIndicator
    {
        id: globalLoaderIndicator

        anchors.fill: parent
        color: parent.color
        enabled: context.globalPreloaderEnabled
        visible: enabled && context.globalPreloaderVisible
    }

    Connections
    {
        target: context

        function onDropConnectingState()
        {
            globalInterfaceLock.enabled = false
        }
    }

    MouseArea
    {
        id: openedTileModalityInterfaceLock

        anchors.fill: parent

        enabled: false
        hoverEnabled: enabled
        acceptedButtons: Qt.LeftButton
        onWheel: wheel.accepted = true
        onClicked:
        {
            grid.closeOpenedTile()
            enabled = false
        }
    }

    MouseArea
    {
        id: globalInterfaceLock

        anchors.fill: parent

        enabled: false
        hoverEnabled: enabled
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onWheel: wheel.accepted = true
        onClicked: mouse.accepted = true
    }
}
