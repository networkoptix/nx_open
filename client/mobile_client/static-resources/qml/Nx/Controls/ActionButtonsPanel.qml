import QtQuick 2.6
import Nx.Models 1.0
import nx.client.mobile 1.0
import Nx 1.0

import "private"

Item
{
    id: control

    property alias resourceId: buttonModel.resourceId
    property alias contentWidth: panel.contentWidth

    signal ptzButtonClicked()

    signal twoWayAudioButtonPressed()
    signal twoWayAudioButtonReleased()

    implicitWidth: panel.implicitWidth
    implicitHeight: panel.implicitHeight

    onResourceIdChanged:
    {
        hintControl.hide()
    }

    ButtonsPanel
    {
        id: panel

        anchors.fill: parent
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
            switch (type)
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

        model: ActionButtonsModel { id: buttonModel }
    }

    QtObject
    {
        id: d

        property variant modelDataAccessor: ModelDataAccessor { model: buttonModel }

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
                        hintControl.showFailure(text, prolonged)
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

                onTriggerCancelled:
                {
                    hintControl.hide()
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
                hintControl.showFailure(text, false)
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
                    hintControl.showFailure(text, true)
            }
            else
            {
                triggersController.deactivateTrigger(id)
                hintControl.hide()
            }
        }
    }

    ActionButtonsHint
    {
        id: hintControl

        x: parent.width - width
        y: -(height + 4 + 4)
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
