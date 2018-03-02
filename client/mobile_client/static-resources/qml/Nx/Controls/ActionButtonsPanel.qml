import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Models 1.0
import com.networkoptix.qml 1.0

import "private"

ButtonsPanel
{
    id: control

    property alias resourceId: buttonModel.resourceId

    signal ptzButtonClicked()

    signal twoWayAudioButtonPressed()
    signal twoWayAudioButtonReleased()

    ActionButtonsHintControl
    {
        id: hintControl

        x: parent.width - width
        y: -(height + 4 + 4)
    }

    onResourceIdChanged:
    {
        //TODO: reset states of buttons
    }

    onPressedChanged:
    {
        var type = d.modelDataAccessor.getData(index, "type")
        if (type == ActionButtonsModel.TwoWayAudioButton)
            d.handleTwoWayAudioPressed(index, pressed)
        else if (type == ActionButtonsModel.SoftTriggerButton)
            d.handleSoftwareTriggerPressed(index, pressed)
    }

    onButtonClicked:
    {
        var type = d.modelDataAccessor.getData(index, "type")
        switch(type)
        {
            case ActionButtonsModel.PtzButton:
                ptzButtonClicked()
                break
            case ActionButtonsModel.TwoWayAudioButton:
                hintControl.showHint(
                    d.modelDataAccessor.getData(index, "hint"),
                    d.modelDataAccessor.getData(index, "iconPath"))
                break
            case ActionButtonsModel.SoftTriggerButton:
                // TODO: temporary. Remove this
                hintControl.showHint(
                    d.modelDataAccessor.getData(index, "hint"),
                    d.modelDataAccessor.getData(index, "iconPath"))
                break
        }
    }

    model:
        ActionButtonsModel
        {
            id: buttonModel
        }

    readonly property QtObject d:
        QtObject
        {
            property variant modelDataAccessor:
                ModelDataAccessor
                {
                    model: buttonModel
                }

            property SoftwareTriggersController triggersController:
                SoftwareTriggersController
                {
                    resourceId: control.resourceId
                }

            function handleTwoWayAudioPressed(index, pressed)
            {
                if (pressed)
                {
                    control.twoWayAudioButtonPressed()
                    hintControl.showCustomProcess(voiceVisualizerComponent,
                        d.modelDataAccessor.getData(index, "iconPath"))
                }
                else
                {
                    control.twoWayAudioButtonReleased()
                    hintControl.hide()
                }
            }

            function handleSoftwareTriggerClicked(index)
            {
                if (modelDataAccessor.getData("prolongedTrigger"))
                    return

                triggersController.activateTrigger(d.modelDataAccessor.getData(index, "id"))
            }

            function handleSoftwareTriggerPressed(index, pressed)
            {
                if (!modelDataAccessor.getData("prolongedTrigger"))
                    return

                triggersController.activateTrigger(d.modelDataAccessor.getData(index, "id"))
            }
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
