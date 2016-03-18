import QtQuick 2.6;
import QtQuick.Controls 1.2;
import NetworkOptix.Qml 1.0;

import "."

Rectangle
{
    id: thisComponent;

    anchors.fill: parent;
    color: Style.colors.window;

    enabled: context.isEnabled;
    focus: true;

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

        anchors.centerIn: parent;

        readonly property int horizontalOffset: 40;
        readonly property int tileWidth: 280;
        property int maxColumns: ((parent.width - 2 * horizontalOffset) / tileWidth)
        property int desiredColumns:(itemsSource.count > 3
            ? ((itemsSource.count + 1) / 2) : itemsSource.count);

        rows: (itemsSource.count > 3 ? 2 : 1);
        columns: Math.min(desiredColumns, maxColumns);

        spacing: 16;

        property QtObject watcher: SingleActiveItemSelector
        {
            variableName: "isExpanded";
            writeVariableName: "isExpandedPrivate";
        }

        Repeater
        {
            id: itemsSource;

            model: QnSystemsModel {}

            delegate: Loader
            {
                id: tileLoader;

                z: (item ? item.z : 0);
                Component
                {
                    id: localSystemTile;

                    LocalSystemTile
                    {
                        visualParent: thisComponent;

                        systemName: model.systemName;
                        isRecentlyConnected: (knownUsersModel ? knownUsersModel.hasConnections : false);

                        correctTile: model.isCompatible;
                        knownUsersModel: QnLastSystemConnectionsData { systemName: model.systemName; }
                        knownHostsModel: model.hostsModel;

                        onConnectClicked:
                        {
                            console.log(selectedHost, ":", selectedUser, ":", selectedPassword);
                            context.connectToLocalSystem(selectedHost, selectedUser, selectedPassword);
                        }
                    }
                }

                Component
                {
                    id: cloudSystemTile;

                    CloudSystemTile
                    {
                        visualParent: thisComponent;

                        systemName: model.systemName;
                        host: model.host;
                        onConnectClicked:
                        {
                            console.log("---", model.host);
                            context.connectToCloudSystem(model.host);
                        }
                    }
                }

                sourceComponent: (model.isCloudSystem
                    ? cloudSystemTile : localSystemTile);

                onLoaded: { grid.watcher.addItem(tileLoader.item); }
            }
        }
    }

    NxButton
    {
        y: (parent.height + height) / 2 + 168 ; // 168 is a magic const by design
        anchors.horizontalCenter: parent.horizontalCenter;

        text: qsTr("Connect to another system");

        onClicked: context.connectToAnotherSystem();
    }

    Keys.onEscapePressed: context.tryHideScreen();
}











