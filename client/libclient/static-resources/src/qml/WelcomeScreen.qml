import QtQuick 2.6;
import QtQuick.Controls 1.2;
import NetworkOptix.Qml 1.0;

import "."

Rectangle
{
    id: control;

    width: context.pageSize.width;
    height: context.pageSize.height;

    color: Style.colors.window;

    Item
    {
        id: screenHolder;

        anchors.fill: parent;
        visible: context.visibleControls;

        CloudPanel
        {
            id: cloudPanel;

            y: ((grid.y - height) / 2);
            anchors.horizontalCenter: parent.horizontalCenter;

            userName: context.cloudUserName;
            loggedIn: context.isLoggedInToCloud;


            onLoginToCloud: context.loginToCloud();
            onCreateAccount: context.createAccount();

            onManageAccount: context.manageCloudAccount();
            onLogout: context.logoutFromCloud();
        }

        GridView
        {
            id: grid;

            readonly property int horizontalOffset: 40;
            readonly property int tileHeight: 96;
            readonly property int tileWidth: 280;
            readonly property int tileSpacing: 16;

            readonly property int maxColsCount: Math.min(Math.floor(parent.width - 2 * horizontalOffset) / cellWidth, 4);
            readonly property int desiredColsCount:
            {
                if (grid.count < 3)
                    return grid.count;
                else if (grid.count < 5)
                    return 2;
                else if (grid.count < 7)
                    return 3;
                else
                    return 4;
            }

            readonly property int colsCount: Math.min(maxColsCount, desiredColsCount);
            readonly property int rowsCount: (grid.count < 3 ? 1 : 2);
            readonly property int pagesCount: Math.ceil(grid.count / (colsCount * rowsCount));

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

                PropertyAction
                {
                    target: grid;
                    property: "contentY";
                    value: grid.contentItem.y + switchPageAnimation.pageIndex * grid.cellHeight * 2;
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
            interactive: true;

            y: Math.max((control.height - height) / 2, 212);
            width: colsCount * cellWidth;
            height: rowsCount * cellHeight;

            cellWidth: tileWidth + tileSpacing;
            cellHeight: tileHeight + tileSpacing;
            cacheBuffer: 64;
            anchors.horizontalCenter: parent.horizontalCenter;

            property QtObject watcher: SingleActiveItemSelector
            {
                variableName: "isExpanded";
                deactivateFunc: function(item) { item.toggle(); };
            }


            Connections
            {
                target: context;
                onIsVisibleChanged:
                {
                    if (!context.isVisible)
                        grid.watcher.resetCurrentItem();
                }
            }

            model: QnSystemsModel {}

            delegate: Item
            {
                z: tile.z;
                width: grid.cellWidth;
                height: grid.cellHeight;

                SystemTile
                {
                    id: tile;
                    visualParent: screenHolder;
                    anchors.horizontalCenter: parent.horizontalCenter;
                    anchors.verticalCenter: parent.verticalCenter;
                    //

                    systemId: model.systemId;
                    systemName: model.systemName;
                    ownerDescription: model.ownerDescription

                    isFactoryTile: model.isFactorySystem;
                    isCloudTile: model.isCloudSystem;

                    wrongVersion: model.wrongVersion;
                    wrongCustomization: model.wrongCustomization;
                    compatibleVersion: model.compatibleVersion;

                    Component.onCompleted: { grid.watcher.addItem(this); }
                }
            }

            function setPage(index)
            {
                switchPageAnimation.stop();
                switchPageAnimation.pageIndex = index;
                switchPageAnimation.start();
            }
        }

        NxPageSwitcher
        {
            id: pageSwitcher;

            visible: (pagesCount > 0);
            anchors.horizontalCenter: grid.horizontalCenter;
            anchors.top: grid.bottom;
            anchors.topMargin: 8;

            pagesCount: grid.pagesCount;

            onCurrentPageChanged:
            {
                if ((currentPage >= 0) && (currentPage < pagesCount))
                    grid.setPage(currentPage);
            }
        }

        NxButton
        {
            anchors.bottom: parent.bottom;
            anchors.bottomMargin: 64;   // Magic const by design
            anchors.horizontalCenter: parent.horizontalCenter;

            text: qsTr("Connect to another system");

            onClicked: context.connectToAnotherSystem();
        }

        Keys.onEscapePressed: context.tryHideScreen();
    }
}









