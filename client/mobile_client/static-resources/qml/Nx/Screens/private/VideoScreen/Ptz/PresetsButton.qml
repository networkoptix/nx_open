import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Button
{
    id: control

    property var uniqueResourceId
    property Item popupParent

    signal presetChoosen(string id)
    height: 48
    flat: true

    text: qsTr("PRESETS")

    onClicked:
    {
        var dialog = Workflow.openDialog(
            "Screens/private/VideoScreen/Ptz/PresetsDialog.qml",
            {
                "uniqueResourceId": control.uniqueResourceId
            })

        dialog.onPresetChoosen.connect(control.presetChoosen)
    }
}
