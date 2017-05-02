import QtQuick 2.6
import Nx.Controls 1.0
import Nx 1.0

Item
{
    id: control

    property int presetsCount: 0
    property int currentPresetIndex: -1

    signal presetsButtonClicked()
    signal goToPreset(int presetIndex)

    implicitWidth: presetsButton.width
    implicitHeight: presetsButton.height

    Button
    {
        id: presetsButton
        height: 48
        flat: true

        text: qsTr("PRESETS")
        onClicked: { control.presetsButtonClicked() }
    }

    Flickable
    {
        id: flickable

        clip: true
        x: presetsButton.width
        width: parent.width - x
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
                    height: presetsButton.height

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
}
