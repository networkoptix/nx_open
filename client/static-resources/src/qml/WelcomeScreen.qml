import QtQuick 2.6;
import QtQuick.Controls 1.2;
import NetworkOptix.Qml 1.0;

import "."

Rectangle
{
    id: thisComponent;

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

        Grid
        {
            id: grid;

            y: Math.max((thisComponent.height - height) / 2, 232);
            anchors.horizontalCenter: parent.horizontalCenter;

            readonly property int horizontalOffset: 40;
            readonly property int tileWidth: 280;
            property int maxColumns: ((parent.width - 2 * horizontalOffset) / tileWidth)
            property int desiredColumns: Math.min(itemsSource.count, 4);

            property QtObject watcher: SingleActiveItemSelector
            {
                variableName: "isExpanded";
                deactivateFunc: function(item) { item.toggle(); };
            }

            rows: (itemsSource.count > 3 ? 2 : 1);
            columns: Math.min(desiredColumns, maxColumns);

            spacing: 16;


            Connections
            {
                target: context;
                onIsVisibleChanged:
                {
                    if (!context.isVisible)
                        grid.watcher.resetCurrentItem();
                }
            }

            Repeater
            {
                id: itemsSource;

                model: QnSystemsModel {}

                delegate: SystemTile
                {
                    visualParent: screenHolder;


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









