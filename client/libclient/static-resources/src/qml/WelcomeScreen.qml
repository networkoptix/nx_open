import QtQuick 2.6;
import QtQuick.Controls 1.2;
import NetworkOptix.Qml 1.0;
import com.networkoptix.qml 1.0;

import "."

Rectangle
{
    id: control;

    width: context.pageSize.width;
    height: context.pageSize.height;

    color: Style.colors.window;

    QnAppInfo
    {
        id: appInfo
    }

    Item
    {
        id: screenHolder;

        anchors.fill: parent;
        visible: context.visibleControls && !context.globalPreloaderVisible;

        Image
        {
            id: statusImage;

            width: 120;
            height: 120;
            y: ((searchEdit.y - height) / 2);
            anchors.horizontalCenter: parent.horizontalCenter;

            source: "qrc:/skin/welcome_page/logo.png"
        }

        NxSearchEdit
        {
            id: searchEdit;
            visible: grid.totalItemsCount > grid.itemsPerPage
            visualParent: screenHolder

            anchors.bottom: gridHolder.top
            anchors.bottomMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter

            onQueryChanged: { grid.model.setFilterWildcard(query); }
        }

        Item
        {
            id: gridHolder;

            y: Math.max((control.height - height) / 2, 212);
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
                readonly property int totalItemsCount: model.totalCount

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
                    variableName: "isExpanded";
                    deactivateFunc: function(item) { item.toggle(); };
                }

                model: QnOrderedSystemsModel
                {
                    filterCaseSensitivity: Qt.CaseInsensitive;
                    filterRole: 257;    // Search text role
                    readonly property int totalCount: rowCount();
                }

                delegate: Item
                {
                    z: tile.z
                    width: grid.cellWidth
                    height: grid.cellHeight

                    SystemTile
                    {
                        id: tile
                        visualParent: screenHolder
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        systemId: model.systemId
                        systemName: model.systemName
                        ownerDescription: model.ownerDescription

                        isFactoryTile: model.isFactorySystem
                        isCloudTile: model.isCloudSystem

                        wrongVersion: model.wrongVersion
                        isCompatibleInternal: model.isCompatibleInternal
                        compatibleVersion: model.compatibleVersion

                        Component.onCompleted: { grid.watcher.addItem(this); }
                    }
                }

                function setPage(index, animate)
                {
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
            }

            NxPageSwitcher
            {
                id: pageSwitcher;

                visible: (pagesCount > 1);
                anchors.horizontalCenter: gridHolder.horizontalCenter;
                anchors.top: gridHolder.bottom;
                anchors.topMargin: 8;

                pagesCount: grid.pagesCount;

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
            visible: foundServersCount == 0;
        }

        NxButton
        {
            anchors.bottom: parent.bottom;
            anchors.bottomMargin: 64;   // Magic const by design
            anchors.horizontalCenter: parent.horizontalCenter;

            text: qsTr("Connect to another system");

            onClicked: context.connectToAnotherSystem();
        }

        NxBanner
        {
            visible: !context.isCloudEnabled;

            anchors.top: parent.top;
            anchors.topMargin: 16;
            anchors.horizontalCenter: parent.horizontalCenter;

            textControl.text: qsTr("You have no access to %1. Some features could be unavailable.").arg(appInfo.cloudName());
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
            color: Style.colors.mid;
            font: Style.fonts.preloader;
            anchors.horizontalCenter: parent.horizontalCenter;
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

    NxLabel
    {
        x: 8;
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 8;

        text: context.softwareVersion;
        standardColor: Style.darkerColor(Style.colors.windowText, 1);
        font: Qt.font({ pixelSize: 11, weight: Font.Normal})
    }

    Connections
    {
        target: context;
        onIsVisibleChanged:
        {
            grid.watcher.resetCurrentItem();
            pageSwitcher.setPage(0);
        }
    }
}
