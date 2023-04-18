// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.0

import Nx 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0
import Nx.JoystickInvestigationWizard 1.0

WizardStep
{
    property alias filePath: filePathField.text

    function updateOutputFilePath()
    {
        joystickInvestigationDialog.outputFilePath = filePath
    }

    Timer
    {
        id: updateTimer

        interval: 1000
        running: false
        repeat: false

        onTriggered: updateOutputFilePath()
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        FileDialog
        {
            id: saveFileDialog

            title: qsTr("Choose file to save")
            nameFilters: [qsTr("JSON files (*.json)")]
            fileMode: FileDialog.SaveFile

            onAccepted: filePath = selectedFile
        }

        Label
        {
            text: qsTr("File to save the results:")
        }

        RowLayout
        {
            width: parent.width

            TextField
            {
                id: filePathField

                Layout.fillWidth: true
                text: ""

                onTextChanged: updateTimer.restart()
                onEditingFinished: updateOutputFilePath()
            }

            Button
            {
                implicitWidth: 30
                text: "..."

                onClicked: saveFileDialog.open()
            }
        }
    }
}
