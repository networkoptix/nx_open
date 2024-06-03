// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.RightPanel

import nx.vms.client.desktop

import ".."

SearchPanel
{
    id: motionPanel

    type: { return RightPanelModel.Type.motion }

    brief: true
    limitToCurrentCamera: true

    placeholder
    {
        icon: "image://skin/64x64/Outline/motion.svg"

        title: placeholderAction.visible
            ? qsTr("Select a camera to see its motion events")
            : qsTr("No motion detected")

        description: placeholderAction.visible
            ? ""
            : qsTr("Try changing the filters or enable motion recording")

        action: Action
        {
            id: placeholderAction

            text: qsTr("Select Camera...")
            icon.source: "image://skin/tree/camera.svg"

            visible: motionPanel.model.isOnline
                && rightPanel.scene
                && rightPanel.scene.itemCount === 0
                && !rightPanel.scene.isLocked
                && !rightPanel.scene.isShowreelReviewLayout

            onTriggered:
                motionPanel.model.addCameraToLayout()
        }
    }

    MotionAreaSelector
    {
        motionModel: motionPanel.model.sourceModel
        parent: motionPanel.filtersColumn
        width: parent.width
    }
}
