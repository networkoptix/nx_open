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

    Grid
    {
        anchors.centerIn: parent;

        rows: (itemsSource.count > 3 ? 2 : 1);
        columns: (itemsSource.count > 3
            ? ((itemsSource.count + 1) / 2) : itemsSource.count);

        spacing: 16;

        Repeater
        {
            id: itemsSource;
            model: QnSystemsModel {}

            delegate: Loader
            {
                Component
                {
                    id: localSystemTile;

                    LocalSystemTile
                    {
                        systemName: model.systemName;
                        host: model.host;
                        userName: model.userName;
                        isComaptible: model.isCompatible;
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











