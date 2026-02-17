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

    title: qsTr("Actions")
    spacing: 24
    contentSpacing: spacing

    onResourceChanged: actionVisualizer.hide()

    ActionVisualizer
    {
        id: actionVisualizer

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
            readonly property bool prolonged: model.type === CameraButton.Type.prolonged
            readonly property string iconSource: model.iconPath
            readonly property string hintText: model.hint + model.name
            readonly property var visualizer:
                model.group === CameraButtonController.ButtonGroup.twoWayAudio
                    ? actionVisualizer.voiceVisualizer
                    : actionVisualizer.defaultVisualizer

            parent: externalMode ? externalButtonContainer : sheetLayout

            icon.width: 24
            icon.height: 24
            icon.source: iconSource
            opacity: model.enabled ? 1.0 : 0.3
            type: externalMode ? Button.Type.Interface : Button.LightInterface
            down: instantActionHandler.pressed || prolongedActionHandler.pressed
            checked: down && prolonged
            clip: true

            implicitHeight: 44
            implicitWidth: 44

            Layout.fillWidth: true

            Item
            {
                id: tapArea

                anchors.fill: parent

                TapHandler
                {
                    id: instantActionHandler

                    enabled: !prolonged && model.enabled

                    onTapped: Qt.callLater(startAction)
                    onPressedChanged:
                    {
                        if (pressed)
                            actionVisualizer.showHint(hintText, iconSource, /*keepOpened*/ true)
                        else
                            actionVisualizer.hide()
                    }
                }

                TapHandler
                {
                    id: prolongedActionHandler

                    enabled: prolonged && model.enabled

                    property bool fullyActivated: false
                    readonly property real fullActivationTimeS: 0.5
                    longPressThreshold: fullActivationTimeS

                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    grabPermissions: PointerHandler.TakeOverForbidden

                    onTapped:
                    {
                        if (!fullyActivated)
                            Qt.callLater(() => actionVisualizer.showHint(hintText, iconSource))
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

                    enabled: !model.enabled
                    onPressedChanged:
                    {
                        if (pressed)
                            actionVisualizer.showHint(qsTr("Disabled by schedule"))
                    }
                }
            }

            function startAction()
            {
                if (!model.enabled)
                    return

                if (!controller.startAction(model.id))
                {
                    actionVisualizer.showFailure(hintText)
                    return
                }

                if (model.group === CameraButtonController.ButtonGroup.twoWayAudio)
                    windowContext.ui.windowHelpers.requestRecordAudioPermissionIfNeeded()

                actionVisualizer.showActivity(visualizer, model.name, iconSource)
            }

            function stopAction()
            {
                if (controller.actionIsActive(model.id))
                    controller.stopAction(model.id)

                actionVisualizer.hide()
            }

            Connections
            {
                target: controller

                function onActionStarted(id, success)
                {
                    if (id !== model.id)
                        return

                    if (!success)
                        actionVisualizer.showFailure(hintText)
                    else if (!prolonged)
                        actionVisualizer.showSuccess(hintText)
                    else if (controller.actionIsActive(model.id))
                        actionVisualizer.showActivity(visualizer, name, iconSource)
                }

                function onActionCancelled(id)
                {
                    actionVisualizer.hide()
                }
            }
        }
    }

    GridLayout
    {
        id: sheetLayout

        width: parent.width
        columns: 5
        columnSpacing: 12
        rowSpacing: 12
    }

    footer: Button
    {
        text: qsTr("Close")
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

            implicitWidth: 44
            implicitHeight: 44

            icon.source: "image://skin/24x24/Outline/grid_view.svg"
            icon.width: 24
            icon.height: 24

            onClicked: sheet.open()
        }
    }
}
