import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0
import Nx.Settings 1.0

Page
{
    id: settingsScreen

    title: qsTr("Developer Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    padding: 16

    DeveloperSettingsHelper
    {
        id: helper
    }

    Button
    {
        text: "Change Log Level: " + helper.logLevel
        onClicked:
        {
            logLevelDialog.open()
        }
    }

    ItemSelectionDialog
    {
        id: logLevelDialog
        title: qsTr("Log Level")
        currentItem: helper.logLevel
        onCurrentItemChanged: helper.logLevel = currentItem
        model: [
            "NONE",
            "ALWAYS",
            "ERROR",
            "WARNING",
            "INFO",
            "DEBUG1",
            "DEBUG2"]
    }
}
