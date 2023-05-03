// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
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
            source: "image://svg/skin/joystick_settings/panAndTilt.svg"
            sourceSize: Qt.size(120, 120)

            transform: Rotation
            {
                angle: 90
                origin.x: joystickImage.width / 2
                origin.y: joystickImage.height / 2
            }
        }
    }

    Connections
    {
        id: connections

        enabled: xAxisStep.StackView.status === StackView.Active
        target: joystickInvestigationDialog.currentDevice

        function onStateChanged(newState)
        {
            joystickInvestigationDialog.collectedData.xAxis = [
                ...joystickInvestigationDialog.collectedData.xAxis,
                newState
            ]
        }
    }
}
