import QtQuick 2.6
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Row
{
    id: control

    property int presetsCount: 0
    property int currentPresetIndex: -1

    Repeater
    {
        id: repeater

        model: control.presetsCount

        delegate: Button
        {
            text: index
            onClicked: { control.currentPresetIndex = index }
        }
    }
}
