// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Settings

import nx.client.mobile

Loader
{
    id: control

    property var action: null
    property bool externalMode: false

    readonly property Component defaultVisualizer: defaultVisualizer
    readonly property Component voiceVisualizer: voiceVisualizer

    active: !!action
    sourceComponent: action?.visualizer || defaultVisualizer
    state: "hidden"

    component ActionVisualizerControl: Control
    {
        implicitHeight: 30
        horizontalPadding: 12

        background: Rectangle
        {
            opacity: externalMode ? 0.7 : 1.0
            radius: 24
            color: externalMode ? "#0D1012" : (action?.color ?? ColorTheme.colors.light1)
        }
    }

    Component
    {
        id: defaultVisualizer

        ActionVisualizerControl
        {
            id: control

            property string text: action.text ?? ""
            property string iconSource: action.icon ?? ""

            contentItem: Row
            {
                spacing: 6

                ColoredImage
                {
                    id: icon

                    sourcePath: control.iconSource
                    sourceSize: Qt.size(20, 20)
                    primaryColor: externalMode ? ColorTheme.colors.light1 : ColorTheme.colors.dark4
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text
                {
                    id: textItem

                    height: parent.height

                    text: control.text
                    color: externalMode ? ColorTheme.colors.light1 : ColorTheme.colors.dark4
                    font.pixelSize: 14
                    verticalAlignment: Qt.AlignVCenter
                }
            }
        }
    }

    Component
    {
        id: voiceVisualizer

        ActionVisualizerControl
        {
            contentItem: VoiceSpectrumItem
            {
                color: externalMode ? ColorTheme.colors.light1 : ColorTheme.colors.dark4
                lineSpacing: 2
                implicitWidth: 142
                implicitHeight: 16
            }
        }
    }

    states:
    [
        State
        {
            name: "visible"
            PropertyChanges { control.opacity: 1 }
        },
        State
        {
            name: "hidden"
            PropertyChanges { control.opacity: 0 }
        }
    ]

    transitions:
    [
        Transition
        {
            from: "visible"
            to: "hidden"

            SequentialAnimation
            {
                NumberAnimation
                {
                    property: "opacity"
                    duration: 160
                }

                ScriptAction { script: action = null }
            }
        },
        Transition
        {
            from: "hidden"
            to: "visible"

            NumberAnimation
            {
                target: control
                property: "opacity"
                duration: 80
            }
        }
    ]

    function show(visualizer, text, icon, color)
    {
        control.action = {
            visualizer: visualizer,
            text: text,
            icon: icon,
            color: color
        }
        state = "visible"
    }

    function showResult(text, icon, color)
    {
        hideTimer.restart()
        show(defaultVisualizer, text, icon, color)
    }

    function showSuccess(text)
    {
        showResult(text, "image://skin/20x20/Outline/check.svg", ColorTheme.colors.green)
    }

    function showFailure(text)
    {
        showResult(text, "image://skin/20x20/Outline/close_medium.svg", ColorTheme.colors.red)
    }

    function showActivity(visualizer, text)
    {
        hideTimer.stop()
        show(visualizer, text, "image://skin/20x20/Outline/loading.svg")
    }

    function showHint(text, icon, keepOpened)
    {
        hideTimer.restart()
        show(defaultVisualizer, text, icon)
    }

    function hideDelayed()
    {
        hideTimer.restart()
    }

    function hide()
    {
        state = "hidden"
    }

    Timer
    {
        id: hideTimer

        interval: appContext?.settings.iniConfigValue("softTriggerTooltipDurationMs") ?? 2000
        onTriggered: control.hide()
    }
}
