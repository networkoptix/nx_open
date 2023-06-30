// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.JoystickInvestigationWizard 1.0

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
            source: "image://svg/skin/joystick_settings/tilt.svg"
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
            joystickInvestigationDialog.collectedData.yAxis = [
                ...joystickInvestigationDialog.collectedData.yAxis,
                newState
            ]
        }
    }
}
