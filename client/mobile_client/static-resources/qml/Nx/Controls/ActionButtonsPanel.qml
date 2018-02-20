import QtQuick 2.6
import Nx.Core 1.0
import Nx.Models 1.0

ButtonsPanel
{
    property alias resourceId: buttonModel.resourceId

    signal ptzButtonClicked()
    signal twoWayAudioButtonClicked()

    signal softTriggerButtonClicked() // TODO: add parameters and prolongetd studd

    onResourceIdChanged:
    {
        // reset states of buttons
    }

    onPressedChanged:
    {
        console.log("-------- pressed: ", index, pressed)
    }

    onButtonClicked:
    {
        console.log("-------- clicked", index)

        var type = d.modelDataAccessor.getData(index, "type")
        switch(type)
        {
            case ActionButtonsModel.PtzButton:
                ptzButtonClicked()
                break
            case ActionButtonsModel.TwoWayAudioButton:
                twoWayAudioButtonClicked()
                break
            case ActionButtonsModel.SoftTriggerButton:
                // TODO: handle stuff
                break;
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
