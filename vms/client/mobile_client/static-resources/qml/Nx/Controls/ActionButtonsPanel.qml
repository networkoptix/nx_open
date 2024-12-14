// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core
import Nx.Models

import nx.client.mobile
import nx.vms.client.core
import nx.vms.client.mobile

import "private"

Item
{
    id: control

    property alias resource: buttonsController.resource
    property alias contentWidth: panel.contentWidth
    property alias buttonsCount: panel.count

    signal ptzButtonClicked()

    implicitWidth: panel.implicitWidth
    implicitHeight: panel.implicitHeight

    onResourceChanged:
    {
        hintControl.hide()
        panel.forceInitialSlideAnimation()
    }

    onOpacityChanged:
    {
        if (d.previousOpacity == 0 && opacity > 0)
            panel.forceInitialSlideAnimation()

        d.previousOpacity = opacity
    }

    Image
    {
        id: line
        source: "qrc:///images/bottom_panel_line.png"
        visible: panel.scrollable
    }

    ButtonsPanel
    {
        id: panel

        x: 1
        width: parent.width - 1
        height: parent.height

        blockMouseEvents: control.opacity != 1

        onButtonDownChanged: (index, down) =>
        {
            // Shows or hide hint according to the button state and parameters.

            if (!d.canShowHint(index))
                return //< Show hint only for appropriate button groups.

            if (down && !d.enabled(index))
            {
                hintControl.showScheduleError()
                return
            }

            if (d.prolonged(index))
                return //< Prolonged actions should be activated on button press without hint.

            if (down)
                hintControl.showHint(d.hint(index), d.name(index), d.iconPath(index), true)
            else
                hintControl.hide()
        }

        onButtonPressed: (index) =>
        {
            // Start any prolonged action here.

            hintControl.hide()

            if (d.enabled(index) && d.prolonged(index))
                d.startButtonAction(index)
        }

        onButtonClicked: (index) =>
        {
            // Starts instant action or shows hint for a prolonged one.

            if (!d.enabled(index))
            {
                hintControl.hideDelayed()
                return
            }

            if (!d.prolonged(index))
            {
                d.startButtonAction(index)
                return
            }

            // Show hint for the prolonged action
            const kKeepHintOpened = true
            d.stopButtonAction(index, kKeepHintOpened)
            hintControl.showHint(d.hint(index), d.name(index), d.iconPath(index))
        }

        onLongPressedChanged: (index, pressed, down) =>
        {
            var prolonged = d.prolonged(index)
            var enabled = d.enabled(index)
            if (!pressed)
            {
                if (!enabled)
                    hintControl.hideDelayed()
                else if (prolonged)
                    d.stopButtonAction(index)
                else
                    d.startButtonAction(index)
            }
            else if (!prolonged && down && enabled && canShowHint(index))
            {
                hintControl.showHint(d.hint(index), d.name(index), d.iconPath(index), true)
            }
        }

        onActionCancelled: (index) =>
        {
            if (d.enabled(index))
                d.stopButtonAction(index)
            else
                hintControl.hideDelayed()
        }

        onButtonEnabledChanged: (index, buttonEnabled) =>
        {
            if (!buttonEnabled && buttonsController.actionIsActive(d.id(index)))
                d.stopButtonAction(index)
        }

        model: CameraButtonsModel
        {
            id: buttonModel

            controller: CameraButtonController
            {
                id: buttonsController

                onActionStarted: (buttonId, success) =>
                {

                    var index = buttonModel.rowById(buttonId)
                    var text = d.name(index)
                    if (!success)
                    {
                        hintControl.showFailure(text)
                    }
                    else if (d.prolonged(index))
                    {
                        if (buttonsController.actionIsActive(d.id(index))
                            && d.group(index) !== CameraButtonController.ButtonGroup.twoWayAudio)
                        {
                            hintControl.showActivity(text, d.iconPath(index))
                        }
                    }
                    else
                    {
                        hintControl.showSuccess(text)
                    }
                }

                onActionCancelled: (buttonId) => hintControl.hide()
            }
        }
    }

    QtObject
    {
        id: d

        property real previousOpacity: control.opacity

        property variant modelDataAccessor: ModelDataAccessor { model: buttonModel }

        function id(index)
        {
            return modelDataAccessor.getData(index, "id")
        }

        function name(index)
        {
            return modelDataAccessor.getData(index, "name")
        }

        function hint(index)
        {
            return modelDataAccessor.getData(index, "hint")
        }

        function iconPath(index)
        {
            return modelDataAccessor.getData(index, "iconPath")
        }

        function prolonged(index)
        {
            return modelDataAccessor.getData(index, "type") === CameraButton.Type.prolonged
        }

        function enabled(index)
        {
            return modelDataAccessor.getData(index, "enabled")
        }

        function group(index)
        {
            return modelDataAccessor.getData(index, "group")
        }

        function canShowHint(index)
        {
            return group(index) !== CameraButtonController.ButtonGroup.twoWayAudio
        }

        function startButtonAction(index)
        {
            if (!buttonsController.startAction(id(index)))
            {
                hintControl.showFailure(name(index))
                return
            }

            switch (d.group(index))
            {
                case CameraButtonController.ButtonGroup.ptz:
                    hintControl.hide()
                    control.ptzButtonClicked()
                    return
                case CameraButtonController.ButtonGroup.twoWayAudio:
                    requestRecordAudioPermissionIfNeeded()
                    hintControl.showCustomProcess(voiceVisualizerComponent, d.iconPath(index))
                    return
                default:
                    hintControl.showPreloader(name(index))
                    return
            }
        }

        function stopButtonAction(index, keepHintOpened)
        {
            const buttonId = d.id(index)
            if (buttonsController.actionIsActive(buttonId) && prolonged(index))
                buttonsController.stopAction(buttonId)

            if (!keepHintOpened)
                hintControl.hide()
        }
    }

    ActionButtonsHint
    {
        id: hintControl

        x: parent.width - width - 4
        y: -(height + 4)
    }

    Component
    {
        id: voiceVisualizerComponent

        VoiceSpectrumItem
        {
            height: 36
            width: 96
            color: ColorTheme.highlight
        }
    }
}
