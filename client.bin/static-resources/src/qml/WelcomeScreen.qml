import QtQuick 2.5;
import QtQuick.Controls 1.2;
import Networkoptix 1.0;

Rectangle
{
    id: thisComponent;

    anchors.fill: parent;
    color: palette.window;
    enabled: context.isEnabled;
    focus: true;

    SystemPalette { id: palette; }

    MouseArea
    {
        anchors.fill: parent;
        onClicked: grid.watcher.resetCurrentItem();
    }

    Grid
    {
        id: grid;

        anchors.centerIn: parent;

        rows: (itemsSource.count > 3 ? 2 : 1);
        columns: (itemsSource.count > 3
            ? ((itemsSource.count + 1) / 2) : itemsSource.count);

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
                        systemName: model.systemName;
                        host: model.host;
                        userName: model.userName;
                        correctTile: model.isCompatible;
                        knownUsersModel: model.lastUsersModel;
                        knownHostsModel: model.hostsModel;

                        onConnectClicked:
                        {
                            console.log(selectedHost, ":", selectedUser, ":", selectedPassword);
                            context.connectToServer(selectedHost, selectedUser, selectedPassword);
                        }
                    }
                }

                Component
                {
                    id: cloudSystemTile;

                    CloudSystemTile
                    {
                        systemName: model.systemName;
                    }
                }

                sourceComponent: (model.isCloudSystem
                    ? cloudSystemTile : localSystemTile);

                onLoaded: grid.watcher.addItem(tileLoader.item);
            }
        }
    }

    Button
    {
        y: (parent.height + height) / 2 + 168 ; // 168 is a magic const by design
        anchors.horizontalCenter: parent.horizontalCenter;

        text: qsTr("Connect to another system");

        onClicked: context.connectToAnotherSystem();
    }

    Keys.onEscapePressed: context.tryHideScreen();
}











