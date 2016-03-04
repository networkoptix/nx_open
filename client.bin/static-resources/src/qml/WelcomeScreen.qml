import QtQuick 2.5;
import QtQuick.Controls 1.2;

Rectangle
{
    anchors.fill: parent;
    color: palette.window;
    enabled: context.isEnabled;
    focus: true;

    SystemPalette { id: palette; }

    Grid
    {
        anchors.centerIn: parent;

        rows: 2;
        columns: 4;

        spacing: 16;

        Repeater
        {
            id: itemsSource;
            model: context.createSystemsModel();

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

                    Item {}
                }

                sourceComponent: (itemsSource.model.isCloudSystem
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











