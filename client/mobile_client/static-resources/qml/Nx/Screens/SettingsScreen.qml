import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Settings 1.0

Page
{
    id: settingsScreen

    title: qsTr("Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    topPadding: 24
    bottomPadding: 16

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: width
        contentHeight: content.height

        Column
        {
            id: content

            width: flickable.width

            LabeledSwitch
            {
                id: livePreviews

                width: parent.width
                text: qsTr("Live previews in the cameras list")
                checked: settings.liveVideoPreviews
                onCheckedChanged: settings.liveVideoPreviews = checked
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Save passwords for servers")
                checked: settings.savePasswords
                onClicked:
                {
                    settings.savePasswords = checked
                    if (checked)
                        return

                    var dialog = Workflow.openStandardDialog(
                        "", qsTr("What to do with currently saved passwords?"),
                        [
                            "Keep",
                            { "id": "DELETE", "text": qsTr("Delete") }
                        ])

                    dialog.buttonClicked.connect(
                        function(buttonId)
                        {
                            if (buttonId == "DELETE")
                                clearSavedPasswords()
                        })

                }
            }
        }
    }
}
