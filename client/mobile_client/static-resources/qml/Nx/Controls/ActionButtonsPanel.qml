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
                d.handleSoftwareTriggerClicked(index)
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

                    onTriggerActivated:
                    {
                        var index = buttonModel.rowById(id)
                        var text = d.modelDataAccessor.getData(index, "hint")
                        var prolonged = d.modelDataAccessor.getData(index, "prolongedTrigger")

                        if (!success)
                            hintControl.showError(text)
                        else if (!prolonged)
                            hintControl.showSuccess(text, false)
                    }

                    onTriggerDeactivated:
                    {
                        var index = buttonModel.rowById(id)
                        var text = d.modelDataAccessor.getData(index, "hint")
                        if (d.modelDataAccessor.getData(index, "prolongedTrigger"))
                            hintControl.showSuccess(text, true)
                    }
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
                var text = d.modelDataAccessor.getData(index, "hint")
                if (modelDataAccessor.getData(index, "prolongedTrigger"))
                {
                    var hintText = qsTr("Press and hold to %1").arg(text)
                    hintControl.showHint(hintText, d.modelDataAccessor.getData(index, "iconPath"))
                    return
                }

                var id = d.modelDataAccessor.getData(index, "id")
                if (triggersController.activateTrigger(id))
                    hintControl.showPreloader(text)
                else
                    hintControl.showError(text, false)
            }

            function handleSoftwareTriggerPressed(index, pressed)
            {
                if (!modelDataAccessor.getData(index, "prolongedTrigger"))
                    return

                var id = d.modelDataAccessor.getData(index, "id")
                if (pressed)
                {
                    var text = d.modelDataAccessor.getData(index, "hint")
                    if (triggersController.activateTrigger(id))
                        hintControl.showPreloader(text)
                    else
                        hintControl.showError(text, d.modelDataAccessor.getData(index, "iconPath"))
                }
                else
                {
                    triggersController.deactivateTrigger(id)
                    hintControl.hide()
                }
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
