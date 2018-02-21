import QtQuick 2.6
import Nx.Core 1.0
import Nx.Models 1.0

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
        // reset states of buttons
    }

    onPressedChanged:
    {
        var type = d.modelDataAccessor.getData(index, "type")
        if (type != ActionButtonsModel.TwoWayAudioButton)
            return;

        if (pressed)
        {
            control.twoWayAudioButtonPressed()
            hintControl.hide();
        }
        else
        {
            control.twoWayAudioButtonReleased()
        }
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
                hintControl.show("Press and hold to speak",
                    d.modelDataAccessor.getData(index, "iconPath"))
                break
            case ActionButtonsModel.SoftTriggerButton:
                // TODO: handle stuff
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
        }
}
