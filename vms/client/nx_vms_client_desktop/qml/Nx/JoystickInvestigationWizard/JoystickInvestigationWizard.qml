// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Controls
import Nx.Dialogs
import Nx.JoystickInvestigationWizard

import nx.client.desktop
import nx.vms.client.desktop

Dialog
{
    id: joystickInvestigationDialog

    property JoystickManager joystickManager: null
    property JoystickDevice currentDevice: null

    property string outputFilePath: ""
    property bool isSavingPermitted: false
    property QtObject collectedData: QtObject {
        property var xAxis: []
        property var yAxis: []
        property var zAxis: []
        property var buttons: {}
    }

    property var steps: [
        initialStep,
        xAxisStep,
        yAxisStep,
        zAxisStep,
        buttonsStep,
        savingResultsStep
    ]

    signal resultReady(string filePath, string data)

    width: minimumWidth
    height: minimumHeight
    minimumWidth: 450
    minimumHeight: 350

    onOutputFilePathChanged: isSavingPermitted = outputFilePath !== ""

    buttonBox: DialogButtonBox
    {
        Button
        {
            text: qsTr("Save")
            enabled: isSavingPermitted
            visible: stepsStack.currentItem.StackView.index === steps.length - 1
            isAccentButton: true
            onClicked: resultReady(outputFilePath, JSON.stringify(collectedData, null, 2))
        }
        Button
        {
            text: qsTr("Previous")
            enabled: stepsStack.currentItem.StackView.index > 0
            onClicked: stepsStack.pop()
        }
        Button
        {
            text: qsTr("Next")
            visible: stepsStack.currentItem.StackView.index < steps.length - 1
            enabled: stepsStack.currentItem.nextEnabled === true
            onClicked:
            {
                stepsStack.currentItem.beforeNext()
                stepsStack.push(steps[stepsStack.currentItem.StackView.index + 1])
            }
        }
    }

    contentItem: StackView
    {
        id: stepsStack

        initialItem: initialStep
        anchors.fill: parent
    }

    Component { id: initialStep; Initial { } }
    Component { id: xAxisStep; XAxis { } }
    Component { id: yAxisStep; YAxis { } }
    Component { id: zAxisStep; ZAxis { } }
    Component { id: buttonsStep; Buttons { } }
    Component { id: savingResultsStep; SavingResults { } }
}
