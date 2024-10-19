// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Mobile
import Nx.Mobile.Controls

import nx.vms.client.core

/**
 * Cameras selection selector. Allows to specify needed cameras of empty list (which means
 * "any camera" value).
 */
OptionSelector
{
    id: control

    text: qsTr("Device")

    unselectedValue: {
        "selection": EventSearch.CameraSelection.all,
        "cameras": []
    }

    // Since for some reason bindings to var property does not work (even when direct change
    // handler works as expected) we need to update values in handler.
    Connections
    {
        target: control

        function onValueChanged()
        {
            control.textValue = EventSearchUtils.cameraSelectionText(value.selection, value.cameras)
            control.isDefaultValue = !value || value.selection === EventSearch.CameraSelection.all
        }

        Component.onCompleted: onValueChanged()
    }

    screenDelegate: Column
    {
        id: content

        spacing: 4

        function apply()
        {
            control.value = d.currentValue()
        }

        function clear()
        {
            camerasModel.selectedIds = []
        }

        Repeater
        {
            id: camerasRepeater

            model: QnCameraListModel
            {
                id: camerasModel

                selectedIds: d.camerasFromValue(control.value)
            }

            delegate: StyledCheckBox
            {
                rightPadding: 18
                height: 56
                width: (parent && parent.width) ?? 0
                text: model.resourceName
                checked: model.checkState === Qt.Checked
                onCheckedChanged: camerasRepeater.model.setSelected(index, checked)
            }
        }

        NxObject
        {
            id: d

            Connections
            {
                target: control

                function onValueChanged()
                {
                    const sameCameras =
                        (l, r) =>
                        {
                            return l.length === r.length && l.every(
                                (leftCamera) =>
                                {
                                    return !!r.find((rightCamera) => leftCamera === rightCamera)
                                })
                        }

                    const cameras = d.camerasFromValue(control.value)
                    if (!sameCameras(camerasModel.selectedIds, cameras))
                        camerasModel.selectedIds = cameras
                }
            }

            Binding
            {
                target: control
                property: "intermediateValue"
                value: d.currentValue()
            }

            function camerasFromValue(value)
            {
                return value && value.selection !== EventSearch.CameraSelection.all
                    ? value.cameras
                    : []
            }

            function currentValue()
            {
                return camerasModel.selectedIds.length
                    ? {selection: EventSearch.CameraSelection.custom, cameras: camerasModel.selectedIds}
                    : {selection: EventSearch.CameraSelection.all, cameras: []}
            }
        }
    }
}
