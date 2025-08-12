// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.JoystickInvestigationWizard

WizardStep
{
    id: xAxisStep

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Label
        {
            Layout.alignment: Qt.AlignTop
            text: qsTr("Move the joystick left and right several times.")
        }

        Image
        {
            id: joystickImage

            Layout.alignment: Qt.AlignHCenter
            source: "image://skin/64x64/Outline/pan.svg?primary=light16&secondary=light10"
            sourceSize: Qt.size(120, 120)
        }
    }

    Connections
    {
        id: connections

        enabled: xAxisStep.StackView.status === StackView.Active
        target: joystickInvestigationDialog.currentDevice

        function onStateChanged(newState)
        {
            if (Qt.platform.os === "windows")
            {
                joystickInvestigationDialog.collectedData.xAxis = [
                    ...joystickInvestigationDialog.collectedData.xAxis,
                    [
                        joystickInvestigationDialog.currentDevice.xAxisPosition(),
                        joystickInvestigationDialog.currentDevice.xAxisDescription()
                    ]
                ]
            }
            else if (Qt.platform.os === "osx")
            {
                joystickInvestigationDialog.collectedData.xAxis = [
                    ...joystickInvestigationDialog.collectedData.xAxis,
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
