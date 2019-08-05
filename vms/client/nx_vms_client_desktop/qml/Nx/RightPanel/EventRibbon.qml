import QtQuick 2.6
import QtQuick.Controls 2.4

ListView
{
    id: view
    spacing: 1

    delegate: Loader
    {
        id: loader

        x: 8
        height: implicitHeight
        width: parent.width - x - 8

        source:
        {
            if (!model || !model.display)
                return "tiles/SeparatorTile.qml"

            return "tiles/InfoTile.qml"
        }

        Connections
        {
            target: loader.item
            ignoreUnknownSignals: true
            onCloseRequested: view.model.removeRow(index)
        }
    }

    header: Item { height: 8; width: parent.width }
    footer: Item { height: 8; width: parent.width }
}
