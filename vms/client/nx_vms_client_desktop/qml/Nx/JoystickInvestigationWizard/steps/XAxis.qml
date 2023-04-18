// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.JoystickInvestigationWizard 1.0

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
