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
            property int desiredColumns:(itemsSource.count > 3
                ? ((itemsSource.count + 1) / 2) : itemsSource.count);

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

                delegate: Loader
                {
                    id: tileLoader;

                    z: (item ? item.z : 0);

                    Component
                    {
                        id: factorySystemTile;

                        FactorySystemTile
                        {
                            property var hostsModel: QnSystemHostsModel { systemId: model.systemId; }

                            visualParent: screenHolder;
                            systemName: qsTr("New System");
                            host: hostsModel.firstHost;

                            onConnectClicked:
                            {
                                console.log("Show wizard for system <", systemName
                                    , ">, host <", host, ">");
                                context.setupFactorySystem(host);
                            }

                            visible: (index < grid.rows * grid.columns);
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

                            visualParent: screenHolder;

                            systemName: model.systemName;
                            isRecentlyConnected: (knownUsersModel ? knownUsersModel.hasConnections : false);

                            isValidVersion: model.isCompatibleVersion;
                            isValidCustomization: model.isCorrectCustomization;
                            notAvailableLabelText:
                            {
                                if (!isValidVersion)
                                    return model.wrongVersion;
                                else if (!isValidCustomization)
                                    return model.wrongCustomization;

                                return "";
                            }

                            isExpandable: model.isCompatible;
                            isAvailable: isExpandable;
                            knownUsersModel: knownUsersModelProp;
                            knownHostsModel: hostsModel;

                            onConnectClicked:
                            {
                                console.log("Connecting to local system <", systemName
                                    , ">, host <", selectedHost, "> with credentials: "
                                    , selectedUser, ":", selectedPassword);
                                context.connectToLocalSystem(selectedHost, selectedUser, selectedPassword);
                            }

                            enabled: (!isExpanded || !context.connectingNow);
                            visible: (index < grid.rows * grid.columns);
                        }
                    }

                    Component
                    {
                        id: cloudSystemTile;

                        CloudSystemTile
                        {
                            property var hostsModel: QnSystemHostsModel { systemId: model.systemId}

                            visualParent: screenHolder;
                            userName: context.cloudUserName;
                            systemName: model.systemName;
                            isOnline: !hostsModel.isEmpty;

                            isValidVersion: model.isCompatibleVersion;
                            isValidCustomization: model.isCorrectCustomization;

                            notAvailableLabelText:
                            {
                                if (!isValidVersion)
                                    return model.wrongVersion;
                                else if (!isValidCustomization)
                                    return model.wrongCustomization;
                                else if (!isOnline)
                                    return "OFFLINE";

                                return "";
                            }


                            onConnectClicked:
                            {
                                console.log("Connecting to cloud system <", systemName
                                    , ">, throug the host <", hostsModel.firstHost, ">");
                                context.connectToCloudSystem(hostsModel.firstHost);
                            }
                            visible: (index < grid.rows * grid.columns);
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
            anchors.bottom: parent.bottom;
            anchors.bottomMargin: 64;   // Magic const by design
            anchors.horizontalCenter: parent.horizontalCenter;

            text: qsTr("Connect to another system");

            onClicked: context.connectToAnotherSystem();
        }

        Keys.onEscapePressed: context.tryHideScreen();
    }
}









