import QtQuick 2.6;
import QtQuick.Controls 1.2;
import QtQuick.Window 2.2;
import Nx 1.0;
import Nx.Models 1.0;
import Nx.WelcomeScreen 1.0;
import com.networkoptix.qml 1.0;

Rectangle
{
    id: control;

    color: ColorTheme.window;

    readonly property bool active: Window.visibility !== Window.Hidden

    Rectangle
    {
        width: parent.width;
        height: 2;
        color: ColorTheme.transparent(ColorTheme.colors.dark1, 0.15);
    }

    Item
    {
        id: screenHolder;

        anchors.fill: parent;
        visible: context.visibleControls && !context.globalPreloaderVisible;

        Image
        {
            width: 320;
            height: 120;
            y: ((searchEdit.y - height) / 2);
            anchors.horizontalCenter: parent.horizontalCenter;
            source: "qrc:/skin/welcome_page/logo.png"
            fillMode: ((sourceSize.height < height) && (sourceSize.width < width)
                ? Image.Pad
                : Image.PreserveAspectFit);
        }


        NxSearchEdit
        {
            id: searchEdit;

            visible: grid.totalItemsCount > grid.itemsPerPage

            anchors.bottom: gridHolder.top
            anchors.bottomMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter
            z: (grid.watcher.isSomeoneActive ? 0 : 1000);
        }

        Item
        {
            id: gridHolder;

            readonly property real singleLineY: (control.height - height) / 2
            y: grid.rowsCount > 1 && searchEdit.visible
                ? Math.max(singleLineY, 212)
                : singleLineY

            height: grid.tileHeight * 2 + grid.tileSpacing;
            width: parent.width;

            GridView
            {
                id: grid;

                keyNavigationWraps: false;

                readonly property int horizontalOffset: 40;
                readonly property int tileHeight: 96;
                readonly property int tileWidth: 280;
                readonly property int tileSpacing: 16;

                readonly property int maxColsCount:
                {
                    return Math.max(1, Math.min(Math.floor(parent.width - 2 * horizontalOffset) / cellWidth, 4));
                }
                readonly property int desiredColsCount:
                {
                    if (grid.count < 3)
                        return Math.max(1, grid.count);
                    else if (grid.count < 5)
                        return 2;
                    else if (grid.count < 7)
                        return 3;
                    else
                        return 4;
                }

                readonly property int colsCount: Math.min(maxColsCount, desiredColsCount)
                readonly property int rowsCount: (grid.count < 3 ? 1 : 2)
                readonly property int itemsPerPage: colsCount * rowsCount
                readonly property int pagesCount: Math.ceil(grid.count / itemsPerPage)
                readonly property int totalItemsCount: (model ? model.sourceRowsCount : 0);

                opacity: 0;
                snapMode: GridView.SnapOneRow;
                cacheBuffer: tileHeight * 32;   // cache of 32 rows;

                SequentialAnimation
                {
                    id: switchPageAnimation;

                    property int pageIndex: -1;

                    NumberAnimation
                    {
                        target: grid;
                        property: "opacity";
                        duration: 200;
                        easing.type: Easing.Linear;
                        to: 0;
                    }

                    ScriptAction
                    {
                        script: { grid.setPositionTo(switchPageAnimation.pageIndex, GridView.Beginning); }
                    }

                    NumberAnimation
                    {
                        target: grid;
                        property: "opacity";
                        duration: 200;
                        easing.type: Easing.Linear;
                        to: 1;
                    }
                }

                clip: true;
                interactive: false;

                anchors.centerIn: parent;
                width: colsCount * cellWidth;
                height: rowsCount * cellHeight;

                cellWidth: tileWidth + tileSpacing;
                cellHeight: tileHeight + tileSpacing;

                anchors.horizontalCenter: parent.horizontalCenter;

                property QtObject watcher: SingleActiveItemSelector
                {
                    id: itemSelector;
                    variableName: "isExpanded";
                    deactivateFunc: function(item) { item.toggle(); };
                }

                Connections
                {
                    // Handles outer signal that expands tile
                    id: openTileHandler;

                    property variant items: [];

                    function addItem(item)
                    {
                        if (items.indexOf(item) == -1)
                            items.push(item);
                    }

                    function removeItem(item)
                    {
                        var index = items.indexOf(item);
                        if (index > -1)
                            items.splice(index, 1); // Removes element
                    }

                    target: context;

                    onSwitchPage: { pageSwitcher.setPage(pageIndex); }

                    onOpenTile:
                    {
                        if (systemId.length == 0)
                        {
                            // Just try to collapse current tile;
                            grid.watcher.resetCurrentItem();
                            return;
                        }

                        var foundItem = null;
                        var count = openTileHandler.items.length;
                        for (var i = 0; i != count; ++i)
                        {
                            var item = openTileHandler.items[i];
                            if (item.systemId == systemId)
                            {
                                foundItem = item;
                                break;
                            }
                        }

                        if (foundItem && !foundItem.isCloudTile && !foundItem.isFactoryTile
                            && !foundItem.isExpanded && foundItem.isAvailable)
                        {
                            foundItem.toggle();
                        }
                    }
                }

                Loader
                {
                    id: modelLoader;

                    active: (control.active && screenHolder.visible);

                    sourceComponent: Component
                    {
                        OrderedSystemsModel
                        {
                            filterWildcard: searchEdit.query
                            filterCaseSensitivity: Qt.CaseInsensitive;
                            filterRole: 257;    // Search text role
                        }
                    }

                    onItemChanged:
                    {
                        if (grid.model)
                        {
                            pageSwitcher.setPage(0);
                            searchEdit.clear();
                        }

                        grid.model = item;
                    }
                }

                delegate: Item
                {
                    z: tile.z
                    width: grid.cellWidth
                    height: grid.cellHeight

                    SystemTile
                    {
                        id: tile
                        view: grid;
                        visualParent: screenHolder
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        systemId: model.systemId
                        localId: model.localId
                        systemName: model.systemName
                        ownerDescription: model.ownerDescription

                        factorySystem: model.isFactorySystem
                        isCloudTile: model.isCloudSystem
                        safeMode: model.safeMode;

                        wrongVersion: (model.wrongVersion && !model.wrongVersion.isNull()
                            && model.wrongVersion.toString()) || "";
                        isCompatible: model.isCompatibleToDesktopClient;
                        compatibleVersion:(model.compatibleVersion
                            && !model.compatibleVersion.isNull()
                            && model.compatibleVersion.toString()) || "";

                        isRunning: model.isRunning;
                        isReachable: model.isReachable;
                        isConnectable: model.isConnectable;

                        Component.onCompleted:
                        {
                            grid.watcher.addItem(this);
                            openTileHandler.addItem(this);
                        }

                        Component.onDestruction:
                        {
                            openTileHandler.removeItem(this);
                        }
                    }
                }

                function setPage(index, animate)
                {
                    /**
                      * TODO: #ynikitenkov add items watcher, refactor
                      * it to don'use openTileHandler's items
                      */
                    openTileHandler.items.forEach(function(item)
                    {
                        item.cancelAnimationOnCollapse();
                    })

                    switchPageAnimation.stop();
                    if (animate || (opacity == 0)) //< Opacity is 0 on first show
                    {
                        switchPageAnimation.pageIndex = index;
                        switchPageAnimation.start();
                    }
                    else
                        grid.setPositionTo(index);
                }

                function setPositionTo(index)
                {
                    var tilesPerPage = grid.colsCount * grid.rowsCount;
                    var firstItemIndex = index * tilesPerPage;
                    grid.positionViewAtIndex(firstItemIndex, GridView.Beginning);
                }

                footer: Item    //< Represents empty bottom space workaround for model with odd lines
                {
                    visible: ((grid.colsCount % 2) == 1)
                        && ((pageSwitcher.page + 1) == pageSwitcher.pagesCount);
                    height: grid.cellHeight;
                    width: grid.width;
                }

                Component.onCompleted: setPage(0)
            }

            NxPageSwitcher
            {
                id: pageSwitcher;

                visible: (pagesCount > 1);
                anchors.horizontalCenter: gridHolder.horizontalCenter;
                anchors.top: gridHolder.bottom;
                anchors.topMargin: 22;

                pagesCount: Math.min(grid.pagesCount, 10); //< 10 pages maximum

                onCurrentPageChanged:
                {
                    if ((index >= 0) && (index < pagesCount))
                        grid.setPage(index, byClick);
                }
            }
        }

        EmptyTilesPlaceholder
        {
            id: emptyTilePreloader;

            anchors.centerIn: parent;
            foundServersCount: grid.count;
            visible: (!grid.model || (grid.model.sourceRowsCount == 0));
        }

        Item
        {
            height: 208;
            width: parent.width;
            anchors.top: searchEdit.bottom;
            anchors.topMargin: 16;
            anchors.horizontalCenter: parent.horizontalCenter;


            visible: (grid.count == 0) && !emptyTilePreloader.visible;

            NxLabel
            {
                anchors.centerIn: parent;
                font: Style.fonts.notFoundMessages.caption;
                color: ColorTheme.windowText;

                text: qsTr("Nothing found");
            }

        }

        NxButton
        {
            anchors.bottom: parent.bottom;
            anchors.bottomMargin: 64;   // Magic const by design
            anchors.horizontalCenter: parent.horizontalCenter;

            text: grid.totalItemsCount > 0
                ? qsTr("Connect to Another Server...")
                : qsTr("Connect to Server...")

            onClicked: context.connectToAnotherSystem();
        }

        NxBanner
        {
            visible: !context.isCloudEnabled && context.isLoggedInToCloud;

            anchors.top: parent.top;
            anchors.topMargin: 16;
            anchors.horizontalCenter: parent.horizontalCenter;

            textControl.text:
                qsTr("You have no access to %1. Some features could be unavailable.")
                    .arg(AppInfo.cloudName());
        }
    }

    Column
    {
        visible: context.globalPreloaderVisible;
        anchors.centerIn: parent;
        spacing: 36;

        NxCirclesPreloader
        {
            id: preloader;
            anchors.horizontalCenter: parent.horizontalCenter;
        }

        NxLabel
        {
            id: preloaderLabel;
            text: qsTr("Loading...");
            color: ColorTheme.mid;
            font: Style.fonts.preloader;
            anchors.horizontalCenter: parent.horizontalCenter;
            anchors.horizontalCenterOffset: 4;
        }
    }

    MouseArea
    {
        id: connectingStateEventOmitter;

        anchors.fill: parent;
        hoverEnabled: true;

        visible: context.connectingToSystem.length;
    }

    DropArea
    {
        anchors.fill: parent;
        onDropped:
        {
            context.makeDrop(drop.urls);
        }

        onEntered:
        {
            drag.accepted = context.isAcceptableDrag(drag.urls);
        }
    }

    Rectangle
    {
        id: messageHolder;
        anchors.centerIn: parent;
        visible: (context.message.length);
        opacity: (visible ? 1.0 : 0.0);

        radius: 2;
        color: ColorTheme.transparent(ColorTheme.colors.brand_d2, 0.8);
        width: messageLabel.implicitWidth;
        height: messageLabel.implicitHeight;

        Behavior on opacity
        {
            PropertyAnimation
            {
                target: messageHolder;
                property: "opacity";
                duration: 200;
            }
        }

        NxLabel
        {
            id: messageLabel;
            anchors.centerIn: parent;
            font: Style.fonts.screenRecording;
            standardColor: ColorTheme.colors.brand_contrast;
            leftPadding: 24;
            rightPadding: leftPadding;
            topPadding: 10;
            bottomPadding: topPadding;
            text: context.message;
        }
    }

    NxLabel
    {
        x: 8;
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8

        text: AppInfo.applicationVersion()
        standardColor: ColorTheme.darker(ColorTheme.windowText, 1)

        font: Qt.font({ pixelSize: 11, weight: Font.Normal})
    }

    onActiveChanged:
    {
        if (grid.watcher.currentItem)
            grid.watcher.currentItem.forceCollapsedState();
    }
}
