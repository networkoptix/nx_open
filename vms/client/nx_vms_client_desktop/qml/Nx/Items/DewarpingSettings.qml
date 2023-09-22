// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Layouts 1.11

import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.core 1.0
import nx.vms.api 1.0

import "private"

Item
{
    id: settings

    /** Preview image source. */
    property AbstractResourceThumbnail previewSource: null

    /** Enable/disable dewarping. */
    property alias dewarpingEnabled: enableSwitch.checked

    /**
     * Camera projection type.
     * By this type it's determined if the image is fisheye or spherical panorama.
     */
    property int cameraProjection: { return Dewarping.CameraProjection.equidistant }

    /** Fisheye-specific properties. */

    /** Relative center of fisheye image ellipse, [0.0 ... 1.0]. */
    property alias centerX: fisheyeSettings.centerX
    property alias centerY: fisheyeSettings.centerY

    /** Relative horizontal radius of fisheye image ellipse, [0.25 ... 0.75]. */
    property alias radius: fisheyeSettings.radius

    /** Absolute horizontal to vertical radius ratio, [0.5 ... 2.0]. */
    property alias stretch: fisheyeSettings.stretch

    /** Fisheye camera roll correction angle. */
    property alias rollCorrectionDegrees: fisheyeSettings.rollCorrectionDegrees

    /** Fisheye camera mounting type. */
    property alias cameraMount: fisheyeSettings.cameraMount

    /** 360VR-specific properties. */

    /** Horizon correction angles. */
    property alias alphaDegrees: sphereSettings.alphaDegrees
    property alias betaDegrees: sphereSettings.betaDegrees

    /** Convenience signal: every parameter change signal is translated into this one. */
    signal dataChanged()

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    enum DewarpingType
    {
        Fisheye,
        Sphere
    }

    ColumnLayout
    {
        id: content

        spacing: 16
        anchors.fill: parent

        RowLayout
        {
            SwitchButton
            {
                id: enableSwitch
                text: qsTr("Dewarping")
            }

            Item { Layout.fillWidth: true }

            ComboBox
            {
                id: typeComboBox

                textRole: "text"
                valueRole: "value"

                enabled: settings.dewarpingEnabled

                model:
                [
                    {text: qsTr("Fisheye"), value: DewarpingSettings.Fisheye},
                    {text: qsTr("360\u00B0 Equirectangular"), value: DewarpingSettings.Sphere},
                ]

                function updateCurrentValue()
                {
                    const requiredValue =
                        settings.cameraProjection === Dewarping.CameraProjection.equirectangular360
                            ? DewarpingSettings.Sphere
                            : DewarpingSettings.Fisheye

                    currentIndex = model.findIndex(
                        function(item)
                        {
                            return item.value === requiredValue
                        })
                }

                onActivated:
                {
                    settings.cameraProjection = (currentValue === DewarpingSettings.Sphere)
                        ? Dewarping.CameraProjection.equirectangular360
                        : fisheyeSettings.lensProjection
                }

                Component.onCompleted:
                    updateCurrentValue()
            }
        }

        FisheyeDewarpingSettings
        {
            id: fisheyeSettings

            Layout.fillWidth: true
            Layout.fillHeight: true
            previewSource: settings.previewSource
            enabled: settings.dewarpingEnabled
            visible: !is360VR()

            onLensProjectionChanged:
            {
                if (!is360VR())
                    settings.cameraProjection = lensProjection
            }
        }

        SphereDewarpingSettings
        {
            id: sphereSettings

            Layout.fillWidth: true
            Layout.fillHeight: true
            previewSource: settings.previewSource
            enabled: settings.dewarpingEnabled
            visible: is360VR()
        }
    }

    onCameraProjectionChanged:
    {
        typeComboBox.updateCurrentValue()

        if (!is360VR())
            fisheyeSettings.lensProjection = cameraProjection

        dataChanged()
    }

    function is360VR()
    {
        return cameraProjection === Dewarping.CameraProjection.equirectangular360
    }

    // Convenience signal translation.
    onDewarpingEnabledChanged: dataChanged()
    onCenterXChanged: !is360VR() && dataChanged()
    onCenterYChanged: !is360VR() && dataChanged()
    onRadiusChanged: !is360VR() && dataChanged()
    onStretchChanged: !is360VR() && dataChanged()
    onRollCorrectionDegreesChanged: !is360VR() && dataChanged()
    onCameraMountChanged: !is360VR() && dataChanged()
    onAlphaDegreesChanged: is360VR() && dataChanged()
    onBetaDegreesChanged: is360VR() && dataChanged()
}
