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

            onInitiallyPressed:
            {
                var type = d.modelDataAccessor.getData(index, "type")
                if (type == ActionButtonsModel.PtzButton)
                    return;

                var text = d.modelDataAccessor.getData(index, "hint")
                hintControl.showHint(
                    qsTr("Press and hold to %1").arg(text),
                    d.modelDataAccessor.getData(index, "iconPath"))
            }

            onPressedChanged:
            {
                var type = d.modelDataAccessor.getData(index, "type")
                if (type == ActionButtonsModel.TwoWayAudioButton)
                    d.handleTwoWayAudioPressed(index, pressed)
                else if (type == ActionButtonsModel.SoftTriggerButton)
                    d.handleSoftwareTriggerPressed(index, pressed)
            }

            onActionCancelled:
            {
                hintControl.hide()
            }

            onButtonClicked:
            {
                var type = d.modelDataAccessor.getData(index, "type")
                if (type == ActionButtonsModel.PtzButton)
                {
                    ptzButtonClicked()
                    return
                }

                hintControl.hideDelayed()
            }

            onEnabledChanged:
            {
                if (buttonEnabled)
                    return

                var id = d.modelDataAccessor.getData(index, "id")
                if (d.triggersController.activeTriggerId() == id)
                    d.triggersController.deactivateTrigger()
            }

            model: ActionButtonsModel { id: buttonModel }
        }

        QtObject
        {
            id: d

            property variant modelDataAccessor: ModelDataAccessor { model: buttonModel }

            property SoftwareTriggersController triggersController: SoftwareTriggersController
            {
                resourceId: control.resourceId

                onTriggerActivated:
                {
                    var index = buttonModel.rowById(id)
                    var text = d.modelDataAccessor.getData(index, "hint")
                    var prolonged = d.modelDataAccessor.getData(index, "allowLongPress")
                    var icon = d.modelDataAccessor.getData(index, "iconPath")
                    if (!success)
                        hintControl.showFailure(text)
                    else if (prolonged)
                        hintControl.showActivity(text, icon)
                    else
                        hintControl.showSuccess(text)
                }

                onTriggerDeactivated:
                {
                    hintControl.hide()
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

            function handleSoftwareTriggerPressed(index, pressed)
            {
                var prolonged = d.modelDataAccessor.getData(index, "allowLongPress")
                var id = d.modelDataAccessor.getData(index, "id")
                if (pressed)
                {
                    var text = d.modelDataAccessor.getData(index, "hint")
                    if (triggersController.activateTrigger(id))
                        hintControl.showPreloader(text)
                    else
                        hintControl.showFailure(text, prolonged)
                }
                else if (prolonged)
                {
                    triggersController.deactivateTrigger(id)
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
