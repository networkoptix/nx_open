// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.JoystickInvestigationWizard

import nx.vms.client.desktop

WizardStep
{
    id: initialStep

    property var devicesList: [/*{ path: , name: }*/]
    property int selectedDeviceIndex

    function onDeviceListChanged()
    {
        const devices = joystickInvestigationDialog.joystickManager.devices()

        devicesList = Object.keys(devices).map(path => ({ path, name: devices[path] }))

        if (devicesList.length === 0)
            nextEnabled = false
    }

    function onJoystickManagerChanged()
    {
        if (!joystickManager)
            return

        joystickManager.deviceListChanged.connect(onDeviceListChanged)

        onDeviceListChanged()
    }

    function onJoystickSelected(index)
    {
        nextEnabled = true
        selectedDeviceIndex = index
    }

    function onBeforeNext()
    {
        beforeNext.disconnect(onBeforeNext)

        joystickInvestigationDialog.currentDevice =
            joystickInvestigationDialog.joystickManager.createDevice(
                devicesList[selectedDeviceIndex].path)
    }

    StackView.onActivated:
    {
        nextEnabled = false

        joystickInvestigationDialog.currentDevice = null

        joystickInvestigationDialog.collectedData.xAxis = []
        joystickInvestigationDialog.collectedData.yAxis = []
        joystickInvestigationDialog.collectedData.zAxis = []
        joystickInvestigationDialog.collectedData.buttons = {}

        onJoystickManagerChanged()

        beforeNext.connect(onBeforeNext)
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Label
        {
            text: (devicesList.length > 0)
                ? qsTr("Select the joystick.")
                : qsTr("No devices connected.")
        }

        ButtonGroup
        {
            id: joystickRadioButtonsGroup
        }

        ListView
        {
            id: joystickList

            spacing: 10
            Layout.leftMargin: 10
            Layout.fillWidth: true
            Layout.preferredHeight: 200

            model: devicesList

            delegate: RadioButton
            {
                text: devicesList[index].name
                onClicked: onJoystickSelected(index)
                ButtonGroup.group: joystickRadioButtonsGroup
            }
        }

        Item
        {
            Layout.fillHeight: true
        }
    }

    Connections
    {
        enabled: initialStep.StackView.status === StackView.Active
        target: joystickInvestigationDialog

        function onJoystickManagerChanged()
        {
            initialStep.onJoystickManagerChanged()
        }
    }
}
