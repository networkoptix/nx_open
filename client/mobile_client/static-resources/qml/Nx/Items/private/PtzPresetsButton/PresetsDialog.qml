import QtQuick 2.6

import Nx 1.0
import Nx.Dialogs 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

DialogBase
{
    id: control

    property var uniqueResourceId

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
                uniqueResourceId: control.uniqueResourceId
            }

            delegate: DialogListItem
            {
                Row
                {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    Text
                    {
                        text: (index + 1) + "."

                        font.bold: true
                        width: 28
                        elide: Text.ElideRight
                        font.pixelSize: 16
                        leftPadding: 0
                        color: ColorTheme.contrast16
                    }

                    Text
                    {
                        text: display

                        elide: Text.ElideRight
                        font.pixelSize: 16
                        leftPadding: 0
                        color: ColorTheme.base4
                    }
                }

                onClicked:
                {
                    control.presetChoosen(id)
                    control.close()
                }
            }
        }
    }
}
