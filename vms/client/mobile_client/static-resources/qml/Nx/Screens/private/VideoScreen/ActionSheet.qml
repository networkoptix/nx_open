// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Mobile.Controls

import nx.vms.client.core
import nx.vms.client.mobile

AdaptiveSheet
{
    id: sheet

    property alias resource: controller.resource

    property var externalVisualizerContainer
    property var externalButtonContainer
    property bool externalMode: actionButtons.count === 1
    readonly property bool hasActions: actionButtons.count > 0
    property bool available: true

    signal unavailableAction()

    title: qsTr("Actions")

    onResourceChanged: hintControl.hide()
    onExternalModeChanged:
    {
        if(externalMode && opened)
            close() //< No actions left to display.
    }

    ActionVisualizer
    {
        id: hintControl

        parent: externalMode ? externalVisualizerContainer : sheet.titleCustomArea
        externalMode: sheet.externalMode
    }

    Instantiator
    {
        id: actionButtons

        model: CameraButtonsModel
        {
            id: actionsModel

            controller: CameraButtonController { id: controller }
        }

        delegate: Button
        {
            id: delegate

            readonly property bool available: model.enabled && sheet.available
            readonly property bool prolonged: model.type === CameraButton.Type.prolonged
            readonly property string iconSource: model.iconPath
            readonly property string hintText: model.hint || model.name

            readonly property var visualizer: sheet.externalMode
                && model.group === CameraButtonController.ButtonGroup.twoWayAudio
                    ? hintControl.voiceVisualizer
                    : hintControl.defaultVisualizer

            readonly property string visualizerText:
                model.group === CameraButtonController.ButtonGroup.twoWayAudio
                    ? qsTr("Speak...")
                    : model.name

            parent: externalMode ? externalButtonContainer : sheetLayout

            text: externalMode ? "" : model.name
            icon.width: 24
            icon.height: 24
            icon.source: iconSource
            opacity: available ? 1.0 : 0.3
            type: externalMode ? Button.Type.Interface : Button.LightInterface
            down: instantActionHandler.pressed || prolongedActionHandler.pressed
            textHorizontalAlignment: Qt.AlignLeft
            clip: true

            implicitWidth: parent.width

            spacing: 8
            leftPadding: externalMode ? 0 : 16
            rightPadding: externalMode ? 0 : 16
            topPadding: externalMode ? 0 : 10
            bottomPadding: externalMode ? 0 : 10

            Item
            {
                id: tapArea

                anchors.fill: parent

                TapHandler
                {
                    id: instantActionHandler

                    enabled: !prolonged && available

                    onTapped: Qt.callLater(startAction)
                    onPressedChanged:
                    {
                        if (pressed)
                            hintControl.showHint(hintText, iconSource, /*keepOpened*/ true)
                        else
                            hintControl.hide()
                    }
                }

                TapHandler
                {
                    id: prolongedActionHandler

                    enabled: prolonged && available

                    property bool fullyActivated: false
                    readonly property real fullActivationTimeS: 0.5
                    longPressThreshold: fullActivationTimeS

                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    grabPermissions: PointerHandler.TakeOverForbidden

                    onTapped:
                    {
                        if (!fullyActivated)
                            Qt.callLater(() => hintControl.showHint(hintText, iconSource))
                    }

                    onPressedChanged:
                    {
                        if (pressed)
                        {
                            fullyActivated = false
                            startAction()
                        }
                        else
                        {
                            stopAction()
                        }
                    }

                    onLongPressed:
                    {
                        fullyActivated = true
                    }
                }

                TapHandler
                {
                    id: disabledActionHandler

                    enabled: !available
                    onPressedChanged:
                    {
                        if (!pressed)
                            return

                        if (!sheet.available)
                            sheet.unavailableAction()
                        else
                            hintControl.showHint(qsTr("Disabled by schedule"))
                    }
                }
            }

            function startAction()
            {
                if (!model.enabled)
                    return

                if (!controller.startAction(model.id))
                {
                    hintControl.showFailure(hintText)
                    return
                }

                if (model.group === CameraButtonController.ButtonGroup.twoWayAudio)
                    windowContext.ui.windowHelpers.requestRecordAudioPermissionIfNeeded()

                hintControl.showActivity(visualizer, visualizerText, iconSource)
            }

            function stopAction()
            {
                if (controller.actionIsActive(model.id))
                    controller.stopAction(model.id)

                hintControl.hide()
            }

            Connections
            {
                target: controller

                function onActionStarted(id, success)
                {
                    if (id !== model.id)
                        return

                    if (!success)
                        hintControl.showFailure(hintText)
                    else if (!prolonged)
                        hintControl.showSuccess(hintText)
                    else if (controller.actionIsActive(model.id) && delegate.down)
                        hintControl.showActivity(visualizer, visualizerText, iconSource)
                }

                function onActionCancelled(id)
                {
                    hintControl.hide()
                }
            }
        }
    }

    ColumnLayout
    {
        id: sheetLayout

        width: parent.width
        spacing: 8
    }

    footer: Button
    {
        text: qsTr("Cancel")
        type: Button.LightInterface
        onClicked: sheet.close()
    }

    NxObject
    {
        Button
        {
            id: defaultActionButton

            visible: !externalMode
            parent: externalMode ? null : externalButtonContainer

            anchors.fill: parent
            opacity: sheet.available ? 1.0 : 0.3

            icon.source: "image://skin/24x24/Outline/grid_view.svg"
            icon.width: 24
            icon.height: 24

            onClicked:
            {
                if (sheet.available)
                    sheet.open()
                else
                    sheet.unavailableAction()
            }
        }
    }
}
