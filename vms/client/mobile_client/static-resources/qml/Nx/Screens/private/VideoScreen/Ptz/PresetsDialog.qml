import QtQuick 2.6

import Nx 1.0
import Nx.Core 1.0
import Nx.Dialogs 1.0
import Nx.Controls 1.0

DialogBase
{
    id: control

    property var resourceId

    signal presetChoosen(string id)
    deleteOnClose: true

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: qsTr("PTZ Presets")
        }

        DialogSeparator
        {
            padding: 0
        }

        Repeater
        {
            model: PtzPresetModel
            {
                resourceId: control.resourceId
            }

            delegate: DialogListItem
            {
                Text
                {
                    id: indexText
                    x: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: (index + 1) + "."

                    font.bold: true
                    width: 28
                    elide: Text.ElideRight
                    font.pixelSize: 16
                    leftPadding: 0
                    color: ColorTheme.contrast16
                }

                text: display
                textColor: ColorTheme.base4
                leftPadding: indexText.x + indexText.width

                onClicked:
                {
                    control.presetChoosen(id)
                    control.close()
                }
            }
        }
    }
}
