// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Core 1.0
import Nx.Items 1.0
import Nx.Controls 1.0

import nx.vms.api 1.0
import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Item
{
    id: fisheyeSettings

    /** Preview image source. */
    property AbstractResourceThumbnail previewSource: null

    /** Relative center, [0.0 ... 1.0]. */
    property real centerX: 0.5
    property real centerY: 0.5

    /** Relative horizontal radius, [0.25 ... 0.75]. */
    property real radius: 0.5

    /** Absolute horizontal to vertical radius ratio, [0.5 ... 2.0]. */
    property real stretch: 1.0

    /** Camera roll correction angle. */
    property real rollCorrectionDegrees: 0.0

    /** Camera mounting type. */
    property int cameraMount: { return Dewarping.FisheyeCameraMount.wall }

    /** Fisheye lens projection type. */
    property int lensProjection: { return Dewarping.CameraProjection.equidistant }

    implicitHeight: content.implicitHeight
    implicitWidth: content.implicitWidth
    opacity: enabled ? 1.0 : 0.3

    ColumnLayout
    {
        id: content
        spacing: 16

        anchors.fill: parent

        Rectangle
        {
            id: previewArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            color: ColorTheme.colors.dark4

            ResourcePreview
            {
                id: preview
                anchors.fill: parent

                source: fisheyeSettings.previewSource
                applyRotation: false

                loadingIndicatorDotSize: 16
                textSize: 24
            }

            FisheyeEllipse
            {
                id: calibrationEllipse

                parent: preview.imageArea
                anchors.fill: parent

                centerX: fisheyeSettings.centerX
                centerY: fisheyeSettings.centerY
                radius: fisheyeSettings.radius
                aspectRatio: fisheyeSettings.stretch

                rollCorrectionDegrees:
                {
                    var angle = fisheyeSettings.rollCorrectionDegrees
                    if (fisheyeSettings.previewSource)
                        angle -= fisheyeSettings.previewSource.rotationQuadrants * 90

                    return angle
                }

                showGrid: showGridCheckbox.checked
                lensProjection: fisheyeSettings.lensProjection

                // Mouse drag implementation.
                MouseArea
                {
                    id: mouseArea
                    anchors.fill: parent

                    hoverEnabled: true

                    onPositionChanged:
                    {
                        if (!pressed)
                            return

                        centerXAnimation.stop()
                        centerYAnimation.stop()

                        fisheyeSettings.centerX = MathUtils.bound(
                            0.0, originalCenter.x + (mouse.x - pressPos.x) / width, 1.0)

                        fisheyeSettings.centerY = MathUtils.bound(
                            0.0, originalCenter.y + (mouse.y - pressPos.y) / height, 1.0)
                    }

                    onPressed:
                    {
                        pressPos = Qt.point(mouse.x, mouse.y)
                        originalCenter = Qt.point(fisheyeSettings.centerX, fisheyeSettings.centerY)
                    }

                    property point pressPos
                    property point originalCenter

                    CursorOverride.active: containsMouse || pressed

                    CursorOverride.shape: pressed
                        ? Qt.ClosedHandCursor
                        : Qt.OpenHandCursor
                }
            }

            FisheyeCalibrator
            {
                id: calibrator

                function errorText(result)
                {
                    switch (result)
                    {
                        case FisheyeCalibrator.Result.errorNotFisheyeImage:
                            return qsTr("Image is not round")

                        case FisheyeCalibrator.Result.errorTooLowLight:
                            return qsTr("Image might be too dim")

                        case FisheyeCalibrator.Result.errorInvalidInput:
                            return qsTr("Invalid input image")

                        default:
                            return ""
                    }
                }

                onFinished:
                {
                    if (result == FisheyeCalibrator.Result.errorInterrupted)
                        return

                    if (result == FisheyeCalibrator.Result.ok)
                    {
                        radiusAnimation.start()
                        centerXAnimation.start()
                        centerYAnimation.start()
                        stretchAnimation.start()
                    }
                    else
                    {
                        MessageBox.exec(MessageBox.Icon.Information,
                            "Auto calibration failed",
                            errorText(result),
                            MessageBox.Ok);
                    }
                }
            }

            CheckBox
            {
                id: showGridCheckbox

                anchors.left: previewArea.left
                anchors.bottom: previewArea.bottom
                anchors.leftMargin: 8
                anchors.bottomMargin: 8

                text: qsTr("Show grid")
                visible: calibrationEllipse.visible

                checked: LocalSettings.value("showFisheyeCalibrationGrid")
                onToggled: LocalSettings.setValue("showFisheyeCalibrationGrid", checked)
            }

            Button
            {
                id: autoCalibrateButton

                anchors.right: previewArea.right
                anchors.bottom: previewArea.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 8

                text: qsTr("Auto Calibration")
                visible: calibrationEllipse.visible

                enabled: previewSource
                    && previewSource.status == AbstractResourceThumbnail.Status.ready
                    && !calibrator.running

                onClicked:
                {
                    if (preview.source && !!preview.source.url)
                        calibrator.analyzeFrameAsync(preview.source.image)
                }
            }
        }

        RowLayout
        {
            id: settingsArea

            width: content.width
            spacing: 24

            Column
            {
                spacing: 16
                Layout.alignment: Qt.AlignTop

                Grid
                {
                    flow: Grid.TopToBottom
                    rows: 2
                    spacing: 8

                    Label { text: qsTr("Mount") }

                    ComboBox
                    {
                        id: mountComboBox

                        textRole: "text"
                        valueRole: "value"

                        model:
                        [
                            {text: qsTr("Ceiling"), value: Dewarping.FisheyeCameraMount.ceiling},
                            {text: qsTr("Wall"), value: Dewarping.FisheyeCameraMount.wall},
                            {text: qsTr("Floor/table"), value: Dewarping.FisheyeCameraMount.table}
                        ]

                        function updateCurrentValue()
                        {
                            currentIndex = model.findIndex(
                                function(item)
                                {
                                    return item.value == fisheyeSettings.cameraMount
                                })
                        }

                        onActivated:
                            fisheyeSettings.cameraMount = currentValue

                        Component.onCompleted:
                            updateCurrentValue()

                        Connections
                        {
                            target: fisheyeSettings

                            function onCameraMountChanged()
                            {
                                mountComboBox.updateCurrentValue()
                            }
                        }
                    }

                    ContextHintLabel
                    {
                        text: qsTr("Angle")
                        contextHintText: qsTr("Camera roll correction")
                        contextHelpTopic: HelpTopic.MainWindow_MediaItem_Dewarping
                    }

                    DoubleSpinBox
                    {
                        id: angleSpinBox

                        dFrom: -180.0
                        dTo: 180.0
                        dStepSize: 1.0
                        dValue: fisheyeSettings.rollCorrectionDegrees //< Initial value.
                        decimals: 1
                        editable: true
                        suffix: " \u00B0" //< Degree sign.

                        onValueModified:
                            fisheyeSettings.rollCorrectionDegrees = dValue

                        Connections
                        {
                            target: fisheyeSettings

                            function onRollCorrectionDegreesChanged()
                            {
                                angleSpinBox.dValue = MathUtils.visualNormalized180(
                                    fisheyeSettings.rollCorrectionDegrees)
                            }
                        }
                    }
                }

                Column
                {
                    spacing: 8

                    Label { text: qsTr("Lens projection") }

                    ComboBox
                    {
                        id: projectionComboBox

                        textRole: "text"
                        valueRole: "value"

                        model:
                        [
                            {text: qsTr("Equidistant"),
                                value: Dewarping.CameraProjection.equidistant},
                            {text: qsTr("Stereographic"),
                                value: Dewarping.CameraProjection.stereographic},
                            {text: qsTr("Equisolid"),
                                value: Dewarping.CameraProjection.equisolid}
                        ]

                        function updateCurrentValue()
                        {
                            currentIndex = model.findIndex(
                                function(item)
                                {
                                    return item.value == fisheyeSettings.lensProjection
                                })
                        }

                        onActivated:
                            fisheyeSettings.lensProjection = currentValue

                        Component.onCompleted:
                            updateCurrentValue()

                        Connections
                        {
                            target: fisheyeSettings

                            function onLensProjectionChanged()
                            {
                                projectionComboBox.updateCurrentValue()
                            }
                        }
                    }
                }
            }

            GridLayout
            {
                columns: 4

                rowSpacing: 8
                columnSpacing: 4

                Layout.alignment: Qt.AlignTop

                Label { text: qsTr("Size"); Layout.alignment: Qt.AlignRight }
                Image
                {
                    source: "image://svg/skin/fisheye/circle_small_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }

                Slider
                {
                    id: radiusSlider
                    Layout.fillWidth: true

                    from: 0.25
                    to: 0.75
                    value: fisheyeSettings.radius //< Initial value.

                    onMoved:
                    {
                        radiusAnimation.stop()
                        fisheyeSettings.radius = value
                    }

                    Connections
                    {
                        target: fisheyeSettings

                        function onRadiusChanged()
                        {
                            radiusSlider.value = fisheyeSettings.radius
                        }
                    }
                }

                Image
                {
                    source: "image://svg/skin/fisheye/circle_big_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }

                Label { text: qsTr("X Offset"); Layout.alignment: Qt.AlignRight }
                Image
                {
                    source: "image://svg/skin/fisheye/arrow_left_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }

                Slider
                {
                    id: centerXSlider
                    Layout.fillWidth: true

                    from: 0.0
                    to: 1.0
                    value: fisheyeSettings.centerX //< Initial value.

                    onMoved:
                    {
                        centerXAnimation.stop()
                        fisheyeSettings.centerX = value
                    }

                    Connections
                    {
                        target: fisheyeSettings

                        function onCenterXChanged()
                        {
                            centerXSlider.value = fisheyeSettings.centerX
                        }
                    }
                }

                Image
                {
                    source: "image://svg/skin/fisheye/arrow_right_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }

                Label { text: qsTr("Y Offset"); Layout.alignment: Qt.AlignRight }
                Image
                {
                    source: "image://svg/skin/fisheye/arrow_down_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }

                Slider
                {
                    id: centerYSlider
                    Layout.fillWidth: true

                    from: 0.0
                    to: 1.0
                    value: 1.0 - fisheyeSettings.centerY //< Initial value.

                    onMoved:
                    {
                        centerYAnimation.stop()
                        fisheyeSettings.centerY = 1.0 - value
                    }

                    Connections
                    {
                        target: fisheyeSettings

                        function onCenterYChanged()
                        {
                            centerYSlider.value = 1.0 - fisheyeSettings.centerY
                        }
                    }
                }

                Image
                {
                    source: "image://svg/skin/fisheye/arrow_up_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }

                Label { text: qsTr("Ellipticity"); Layout.alignment: Qt.AlignRight }
                Image
                {
                    source: "image://svg/skin/fisheye/ellipse_vertical_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }

                Slider
                {
                    id: stretchSlider
                    Layout.fillWidth: true

                    property real targetFrom: 0.5
                    property real targetTo: 2.0

                    from: MathUtils.log(2.0, targetFrom)
                    to: MathUtils.log(2.0, targetTo)
                    value: MathUtils.log(2.0, fisheyeSettings.stretch) //< Initial value.

                    onMoved:
                    {
                        stretchAnimation.stop()
                        fisheyeSettings.stretch = Math.pow(2.0, value)
                    }

                    Connections
                    {
                        target: fisheyeSettings

                        function onStretchChanged()
                        {
                            stretchSlider.value = MathUtils.log(2.0, MathUtils.bound(
                                stretchSlider.targetFrom,
                                fisheyeSettings.stretch,
                                stretchSlider.targetTo))
                        }
                    }
                }

                Image
                {
                    source: "image://svg/skin/fisheye/ellipse_horizontal_24.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }
            }
        }
    }

    NumberAnimation
    {
        id: centerXAnimation

        duration: 250
        easing.type: Easing.InOutQuad
        target: fisheyeSettings
        property: "centerX"
        to: MathUtils.bound(centerXSlider.from, calibrator.center.x, centerXSlider.to)
    }

    NumberAnimation
    {
        id: centerYAnimation

        duration: 250
        easing.type: Easing.InOutQuad
        target: fisheyeSettings
        property: "centerY"
        to: MathUtils.bound(centerYSlider.from, calibrator.center.y, centerYSlider.to)
    }

    NumberAnimation
    {
        id: radiusAnimation

        duration: 250
        easing.type: Easing.InOutQuad
        target: fisheyeSettings
        property: "radius"
        to: MathUtils.bound(radiusSlider.from, calibrator.radius, radiusSlider.to)
    }

    NumberAnimation
    {
        id: stretchAnimation

        duration: 250
        easing.type: Easing.InOutQuad
        target: fisheyeSettings
        property: "stretch"
        to: MathUtils.bound(stretchSlider.targetFrom, calibrator.stretch, stretchSlider.targetTo)
    }
}
