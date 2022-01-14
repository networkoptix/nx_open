// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.6

import Nx.Items 1.0

import nx.vms.client.core 1.0

Control
{
    id: control

    property var store: null
    property alias previewSource: settings.previewSource
    property alias helpTopic: settings.helpTopic

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
