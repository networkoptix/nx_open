// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.JoystickInvestigationWizard

WizardStep
{
    id: yAxisStep

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Label
        {
            Layout.alignment: Qt.AlignTop
            text: qsTr("Move the joystick up and down several times.")
        }

        Image
        {
            Layout.alignment: Qt.AlignHCenter
            source: "image://skin/64x64/Outline/tilt.svg?primary=light16&secondary=light10"
            sourceSize: Qt.size(120, 120)
        }
    }

    Connections
    {
        id: connections

        enabled: yAxisStep.StackView.status === StackView.Active
        target: joystickInvestigationDialog.currentDevice

        function onStateChanged(newState)
        {
            if (Qt.platform.os === "windows")
            {
                joystickInvestigationDialog.collectedData.yAxis = [
                    ...joystickInvestigationDialog.collectedData.yAxis,
                    [
                        joystickInvestigationDialog.currentDevice.yAxisPosition(),
                        joystickInvestigationDialog.currentDevice.yAxisDescription()
                    ]
                ]
            }
            else if (Qt.platform.os === "osx")
            {
                joystickInvestigationDialog.collectedData.yAxis = [
                    ...joystickInvestigationDialog.collectedData.yAxis,
                    newState
                ]
            }
            else
            {
                console.log("%1 is not supported".arg(Qt.platform.os));
            }
        }
    }
}
