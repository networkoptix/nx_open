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

    signal softTriggerButtonClicked() // TODO: add parameters and prolongetd studd

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
            d.handleTwoWayAudioPressed(pressed)
        else if (type == ActionButtonsModel.SoftTriggerButton)
            d.handleSoftwareTriggerPressed(pressed)
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
                hintControl.showHint("Press and hold to speak",
                    d.modelDataAccessor.getData(index, "iconPath"))
                break
            case ActionButtonsModel.SoftTriggerButton:
                // TODO: temporary. Remove this
                hintControl.showHint(
                    d.modelDataAccessor.getData(index, "triggerName"),
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

            function handleTwoWayAudioPressed(pressed)
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

            function handleSoftwareTriggerPressed(pressed)
            {

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
