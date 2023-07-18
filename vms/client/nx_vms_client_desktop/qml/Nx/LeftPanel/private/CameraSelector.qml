// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

SelectableTextButton
{
    id: cameraSelector

    property CommonObjectSearchSetup setup
    property bool limitToCurrentCamera: false

    selectable: false
    deactivatable: !limitToCurrentCamera
    accented: setup && setup.cameraSelection === RightPanel.CameraSelection.current
    icon.source: "image://svg/skin/text_buttons/camera_20.svg"

    readonly property var actionNames:
    {
        const mixedDevices = setup && setup.mixedDevices
        let result = {}

        result[RightPanel.CameraSelection.all] =
            mixedDevices ? qsTr("Any device") : qsTr("Any camera")

        result[RightPanel.CameraSelection.layout] =
            mixedDevices ? qsTr("Devices on layout") : qsTr("Cameras on layout")

        result[RightPanel.CameraSelection.current] =
            mixedDevices ? qsTr("Selected device") : qsTr("Selected camera")

        result[RightPanel.CameraSelection.custom] =
            mixedDevices ? qsTr("Choose devices...") : qsTr("Choose cameras...")

        return result
    }

    text:
    {
        if (!setup)
            return ""

        function singleCameraText(base, camera)
        {
            const kNdash = "\u2013"
            const name = camera ? camera.name : qsTr("none")
            return `${base} ${kNdash} ${name}`
        }

        switch (setup.cameraSelection)
        {
            case RightPanel.CameraSelection.custom:
            {
                if (setup.cameraCount > 1)
                {
                    return setup.mixedDevices
                        ? qsTr("%n chosen devices", "", setup.cameraCount)
                        : qsTr("%n chosen cameras", "", setup.cameraCount)
                }

                const base = setup.mixedDevices ? qsTr("Chosen device") : qsTr("Chosen camera")
                return singleCameraText(base, setup.singleCamera)
            }

            case RightPanel.CameraSelection.current:
                return singleCameraText(actionNames[setup.cameraSelection], setup.singleCamera)

            default:
                return actionNames[setup.cameraSelection]
        }
    }

    function updateState()
    {
        cameraSelector.setState(
            !setup || setup.cameraSelection === RightPanel.CameraSelection.all
                ? SelectableTextButton.State.Deactivated
                : SelectableTextButton.State.Unselected)
    }

    onDeactivated:
        defaultAction.trigger()

    Component.onCompleted:
        updateState()

    Connections
    {
        target: setup

        function onCameraSelectionChanged()
        {
            cameraSelector.updateState()
        }
    }

    menu: limitToCurrentCamera ? null : menuControl

    Menu
    {
        id: menuControl

        component MenuAction: Action { text: cameraSelector.actionNames[data] }

        MenuAction { data: RightPanel.CameraSelection.layout }
        MenuAction { data: RightPanel.CameraSelection.current }
        MenuSeparator {}
        MenuAction { data: RightPanel.CameraSelection.custom }
        MenuSeparator {}
        MenuAction { id: defaultAction; data: RightPanel.CameraSelection.all }

        onTriggered: (action) =>
        {
            if (!setup)
                return

            if (action.data === RightPanel.CameraSelection.custom)
                Qt.callLater(setup.chooseCustomCameras)
            else
                setup.cameraSelection = action.data
        }
    }
}
