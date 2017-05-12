import QtQuick 2.6
import Nx.Controls 1.0
import Nx 1.0

Flickable
{
    id: control

    property int presetsCount: 0
    property int currentPresetIndex: -1

    signal goToPreset(int presetIndex)

    clip: true
    width: parent.width
    height: parent.height

    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: presetButtonRow.width
    contentHeight: presetButtonRow.height

    Row
    {
        id: presetButtonRow

        spacing: 0

        Repeater
        {
            id: repeater

            model: control.presetsCount

            delegate: MouseArea
            {
                width: 32
                height: 56

                Text
                {
                    anchors.centerIn: parent

                    color: index == currentPresetIndex
                        ? ColorTheme.windowText
                        : ColorTheme.transparent(ColorTheme.windowText, 0.2)

                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    text: index + 1
                }

                onClicked: { control.goToPreset(index) }
            }
        }
    }
}
