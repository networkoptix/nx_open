// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

Control
{
    id: control

    property var store: null
    property alias previewSource: settings.previewSource

    padding: 16

    contentItem: DewarpingSettings
    {
        id: settings

        onDataChanged:
            updateStore()

        Connections
        {
            target: control.store

            function onStateModified()
            {
                if (control.store.resourceId().isNull())
                    return

                var params = control.store.dewarpingParams()
                settings.dewarpingEnabled = params.enabled
                settings.enabled = !store.isReadOnly()
                settings.centerX = params.xCenter
                settings.centerY = params.yCenter
                settings.radius = params.radius
                settings.stretch = params.hStretch
                settings.rollCorrectionDegrees = params.fovRot
                settings.cameraMount = params.viewMode
                settings.cameraProjection = params.cameraProjection
                settings.alphaDegrees = params.sphereAlpha
                settings.betaDegrees = params.sphereBeta
            }
        }

        function updateStore()
        {
            if (!control.store)
                return

            control.store.setDewarpingParams(dewarpingEnabled, centerX, centerY, radius, stretch,
                rollCorrectionDegrees, cameraMount, cameraProjection, alphaDegrees, betaDegrees)
        }
    }
}
