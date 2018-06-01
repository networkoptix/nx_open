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
    property alias buttonsCount: panel.count
    signal ptzButtonClicked()

    signal twoWayAudioButtonPressed()
    signal twoWayAudioButtonReleased()

    implicitWidth: panel.implicitWidth
    implicitHeight: panel.implicitHeight

    onResourceIdChanged:
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

        onButtonDownChanged:
        {
            if (d.getType(index) != ActionButtonsModel.SoftTriggerButton || d.isProlonged(index))
                return

            if (!down)
                hintControl.hide()
        }

        onButtonPressed:
        {
            hintControl.hide()
            switch (d.getType(index))
            {
                case ActionButtonsModel.PtzButton:
                    return
                case ActionButtonsModel.TwoWayAudioButton:
                    control.twoWayAudioButtonPressed()
                    hintControl.showCustomProcess(voiceVisualizerComponent, d.getIcon(index))
                    return
                case ActionButtonsModel.SoftTriggerButton:
                    if (d.isProlonged(index))
                        d.tryActivateTrigger(index)
                    return
                default:
                    throw "Shouldn't get here"
            }
        }

        onButtonClicked:
        {
            switch (d.getType(index))
            {
                case ActionButtonsModel.PtzButton:
                    control.ptzButtonClicked()
                    return
                case ActionButtonsModel.TwoWayAudioButton:
                case ActionButtonsModel.SoftTriggerButton:
                    d.handleTriggerClicked(index)
                    return
                default:
                    throw "Shouldn't get here"
            }
        }

        onLongPressedChanged:
        {
            switch (d.getType(index))
            {
                case ActionButtonsModel.PtzButton:
                    return
                case ActionButtonsModel.TwoWayAudioButton:
                    if (!pressed)
                    {
                        control.twoWayAudioButtonReleased()
                        hintControl.hide()
                    }
                    return
                case ActionButtonsModel.SoftTriggerButton:
                    var prolonged = d.isProlonged(index)

                    if (!pressed)
                    {
                        if (prolonged)
                            d.tryDeactivateTrigger(index)
                        else
                            d.tryActivateTrigger(index)
                    }
                    else if (!d.isProlonged(index))
                    {
                        hintControl.showHint(d.getText(index), d.getIcon(index), true)
                    }
                    return
                default:
                    throw "Shouldn't get here"
            }
        }

        onActionCancelled: d.tryDeactivateTrigger(index)

        onEnabledChanged:
        {
            if (!buttonEnabled && d.triggersController.activeTriggerId() == d.getId(index))
                d.tryDeactivateTrigger(index)
        }

        model: ActionButtonsModel { id: buttonModel }
    }

    QtObject
    {
        id: d

        property real previousOpacity: control.opacity

        property variant modelDataAccessor: ModelDataAccessor { model: buttonModel }

        property SoftwareTriggersController triggersController: SoftwareTriggersController
        {
            resourceId: control.resourceId

            onTriggerActivated:
            {
                var index = buttonModel.rowById(id)
                var text = d.getText(index)
                if (!success)
                {
                    hintControl.showFailure(text)
                }
                else if (d.isProlonged(index))
                {
                    if (d.triggersController.hasActiveTrigger())
                    hintControl.showActivity(text, d.getIcon(index))
                }
                else
                {
                    hintControl.showSuccess(text)
                }
            }

            onTriggerCancelled: hintControl.hide()
        }

        function isProlonged(index)
        {
            return d.modelDataAccessor.getData(index, "prolongedAction")
        }

        function getType(index)
        {
            return d.modelDataAccessor.getData(index, "type")
        }

        function getId(index)
        {
            return d.modelDataAccessor.getData(index, "id")
        }

        function getText(index)
        {
            return d.modelDataAccessor.getData(index, "hint")
        }

        function getIcon(index)
        {
            return d.modelDataAccessor.getData(index, "iconPath")
        }

        function tryActivateTrigger(index)
        {
            var text = getText(index)
            if (triggersController.activateTrigger(getId(index)))
                hintControl.showPreloader(text)
            else
                hintControl.showFailure(text)
        }

        function tryDeactivateTrigger(index, keepHintOpened)
        {
            if (isProlonged(index))
                triggersController.deactivateTrigger()

            if (!keepHintOpened)
                hintControl.hide()
        }

        function handleTriggerClicked(index)
        {
            if (isProlonged(index))
            {
                d.tryDeactivateTrigger(index, true)
                hintControl.showHint(getText(index), getIcon(index))
            }
            else
            {
                tryActivateTrigger(index)
            }
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
