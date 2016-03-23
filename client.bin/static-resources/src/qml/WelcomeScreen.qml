import QtQuick 2.6;
import QtQuick.Controls 1.2;
import NetworkOptix.Qml 1.0;

import "."

Rectangle
{
    id: thisComponent;

    color: Style.colors.window;

    width: context.pageSize.width;
    height: context.pageSize.height;

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
                    id: factorySystemTile;

                    FactorySystemTile
                    {
                        property var hostsModel: QnSystemHostsModel { systemId: model.systemId}
                        visualParent: thisComponent;
                        host: hostsModel.firstHost;
                        systemName: qsTr("New System");

                        onConnectClicked:
                        {
                            context.setupFactorySystem(model.host);
                        }
                    }
                }

                Component
                {
                    id: localSystemTile;

                    LocalSystemTile
                    {
                        property var hostsModel: QnSystemHostsModel { systemId: model.systemId}
                        property var knownUsersModelProp:
                            QnLastSystemConnectionsData { systemName: model.systemName; }

                        visualParent: thisComponent;

                        systemName: model.systemName;
                        isRecentlyConnected: (knownUsersModel ? knownUsersModel.hasConnections : false);

                        allowExpanding: model.isCompatible;
                        knownUsersModel: knownUsersModelProp;
                        knownHostsModel: hostsModel;

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

                        onConnectClicked:
                        {
                            context.connectToCloudSystem(model.host);
                        }
                    }
                }

                sourceComponent: (model.isFactorySystem ? factorySystemTile
                    : (model.isCloudSystem ? cloudSystemTile : localSystemTile));

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











