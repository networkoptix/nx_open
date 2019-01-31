import QtQuick 2.6
import Nx.Controls 1.0
import Nx 1.0

Flickable
{
    id: control

    property int presetsCount: 0
    property int currentPresetIndex: -1

    signal goToPreset(int index)

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

            delegate: Button
            {
                text: index + 1

                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                padding: 0
                width: 32
                height: 48

                color: "transparent"
                textColor: index == currentPresetIndex
                    ? ColorTheme.windowText
                    : ColorTheme.transparent(ColorTheme.windowText, 0.2)

                font.pixelSize: 16
                font.weight: Font.DemiBold

                onClicked: control.goToPreset(index)
            }
        }
    }
}
