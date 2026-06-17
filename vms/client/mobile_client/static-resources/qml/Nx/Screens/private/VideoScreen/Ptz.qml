// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Common
import Nx.Core
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Ui

import nx.vms.api

import "Ptz"

Item
{
    id: control

    required property ScalableVideo targetVideo
    required property Resource resource
    property alias customRotation: panel.customRotation

    property bool active: false
    property bool overlayStyle: false

    readonly property bool available: controller.available ?? false
    readonly property bool moveOnTapMode: state === "moveOnTap"
    property alias sheet: sheet

    signal closed()

    function reset() { state = "default" }
    function openPresetList() { state = "presets" }
    function openMoveOnTap() { state = "moveOnTap" }

    function selectPreset(index)
    {
        presetViewModel.setCurrentPreset(index)
        reset()
    }

    state: "default"

    onActiveChanged:
    {
        if (active)
            reset()
    }

    AdaptiveSheet
    {
        id: sheet

        readonly property Item currentPage:
        {
            if (control.state === "presets")
                return presetSheetPage

            if (control.state === "moveOnTap")
                return control.overlayStyle ? null : moveOnTapSheetPage

            return control.overlayStyle ? null : panelSheetPage
        }

        visible: control.active && !!currentPage
        opacity: control.opacity
        title: currentPage?.title ?? qsTr("PTZ")
        alwaysShowCloseButton: !footer

        contentSpacing: 0

        interactive: !panel.joystick.active
        closePolicy: Popup.NoAutoClose
        modal: false

        onClosed:
        {
            if (control.overlayStyle)
                return

            control.closed()
        }

        Overlay.modeless: Item { }

        StackLayout
        {
            id: sheetContent

            readonly property int preferredHeight: panelSheetPage.implicitHeight
                - (sheet.footer ? sheet.footer.height + sheet.spacing : 0) - sheet.bottomPadding

            currentIndex: sheet.currentPage?.StackLayout.index ?? -1

            width: parent.width
            height: LayoutController.isPortrait
                ? Math.max(sheet.currentPage?.implicitHeight ?? 0, preferredHeight)
                : Math.max(sheet.currentPage?.implicitHeight ?? 0, sheet.availableContentHeight)

            ColumnLayout
            {
                id: panelSheetPage

                spacing: 20

                LayoutItemProxy
                {
                    id: sheetPanel

                    target: panel

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Rectangle
                {
                    id: separator

                    color: ColorTheme.colors.dark12
                    visible: presetViewModel.hasPresets

                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.leftMargin: -24
                    Layout.rightMargin: -24
                }

                PresetSwitch
                {
                    id: sheetPresetSwitch

                    model: presetViewModel.presets
                    currentIndex: presetViewModel.currentPresetIndex
                    visible: presetViewModel.hasPresets

                    Layout.fillWidth: true

                    onClicked: control.openPresetList()
                    onSelected: (index) => control.selectPreset(index)
                }
            }

            MoveOnTapPage
            {
                id: moveOnTapSheetPage

                readonly property string cancelButtonText: qsTr("Cancel Re-Centering")
            }

            PresetList
            {
                id: presetSheetPage

                readonly property string title: qsTr("PTZ Presets")
                readonly property string cancelButtonText: qsTr("Cancel")

                model: presetViewModel.presets
                currentIndex: presetViewModel.currentPresetIndex
                onSelected: (index) => control.selectPreset(index)
            }
        }

        footer: cancelButton.text ? cancelButton : null

        Button
        {
            id: cancelButton

            text: sheet.currentPage?.cancelButtonText ?? ""
            type: Button.LightInterface
            visible: false

            onClicked: control.reset()
        }
    }

    LayoutItemProxy
    {
        id: videoPanel

        parent: targetVideo
        anchors.fill: parent
        anchors.margins: 16

        target: panel
        visible: control.active && control.state === "default" && control.overlayStyle
        opacity: control.opacity

        Button
        {
            id: closeButton

            anchors.top: parent.top
            anchors.right: parent.right

            width: 48
            height: 48
            type: Button.Type.Interface
            foregroundColor: ColorTheme.colors.light4
            backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
            icon.source: "image://skin/24x24/Outline/close.svg"
            icon.width: 24
            icon.height: 24

            onClicked: control.closed()
        }
    }

    PtzPanel
    {
        id: panel

        controller: controller

        onMoveOnTapClicked:
        {
            if (control.state === "moveOnTap")
                control.reset()
            else
                control.openMoveOnTap()
        }
    }

    PresetSwitch
    {
        id: videoPresetSwitch

        parent: targetVideo
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 16

        visible: control.active
            && presetViewModel.hasPresets
            && control.state === "default"
            && control.overlayStyle

        opacity: control.opacity
        overlayStyle: true

        model: presetViewModel.presets
        currentIndex: presetViewModel.currentPresetIndex
        onClicked: control.openPresetList()
        onSelected: (index) => control.selectPreset(index)
    }

    MouseArea
    {
        id: moveOnTapOverlay

        parent: targetVideo
        anchors.fill: parent

        visible: control.active && control.state === "moveOnTap"
        opacity: control.opacity

        MoveOnTapBanner
        {
            visible: control.overlayStyle

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.margins: 16

            onCloseClicked: control.reset()
        }

        onClicked: (mouse) =>
        {
            const position = Qt.point(mouse.x, mouse.y)
            if (resourceHelper.fisheyeParams.enabled)
                return

            const data = targetVideo.getMoveViewportData(position)
            if (!data)
                return

            controller.viewportMove(data.aspect, data.viewport, /*speed*/ 1)
            moveOnTapPreloader.pos = position
            moveOnTapPreloader.visible = true

            control.reset()
        }
    }

    PtzViewportMovePreloader
    {
        id: moveOnTapPreloader

        parent: targetVideo
        visible: false
    }

    PreloadersPanel
    {
        id: preloadersPanel

        parent: targetVideo
        height: targetVideo.fitSize ? targetVideo.fitSize.height : 0
        x: (parent.width - width) / 2
        y: (parent.height - height) / 3

        focusPressed: panel.focusInPressed || panel.focusOutPressed
        zoomInPressed: panel.zoomInPressed
        zoomOutPressed: panel.zoomOutPressed
        moveDirection: panel.moveDirection
        customRotation: panel.customRotation

        Connections
        {
            target: panel

            function onAutoFocusClicked() { preloadersPanel.showCommonPreloader() }
        }

        Connections
        {
            target: presetViewModel

            function onSelected() { preloadersPanel.showCommonPreloader() }
        }
    }

    NxObject
    {
        id: presetViewModel

        property alias presets: presets
        property int currentPresetIndex: -1
        property bool hasPresets: controller.presetsCount > 0 && controller.supportsPresets

        property int presetActivationTimeoutMs: 10000

        signal selected(int index)

        Binding on currentPresetIndex
        {
            when: !presetActivationTimer.running
            value: controller.activePresetIndex
        }

        function setCurrentPreset(index)
        {
            currentPresetIndex = index
            if (index === -1)
                return

            controller.setPresetByIndex(index)
            presetActivationTimer.restart()
            selected(index)
        }

        Timer
        {
            id: presetActivationTimer

            interval: presetViewModel.presetActivationTimeoutMs
        }

        PtzPresetModel
        {
            id: presets

            resource: controller.supportsPresets ? control.resource : null
        }
    }

    PtzController
    {
        id: controller

        resource: control.resource

        property bool supportsPresets: capabilities & PtzAPI.Capability.presets
        property bool supportsMoveOnTap: capabilities & PtzAPI.Capability.viewport
    }

    MediaResourceHelper
    {
        id: resourceHelper

        resource: control.resource
    }
}
