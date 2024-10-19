// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Core.Items 1.0
import Nx.Core.Controls 1.0

Rectangle
{
    id: control

    property int horizontalPadding: 12
    property int verticalPadding: 12

    width: bodyRow.width + horizontalPadding
    height: bodyRow.height + verticalPadding * 2
    color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.95)
    radius: 2

    opacity: 0
    visible: opacity > 0

    states: [
        State
        {
            name: "visible"
            PropertyChanges { target: control; opacity: 1 }
        },

        State
        {
            name: "hidden"
            PropertyChanges { target: control; opacity: 0 }
        }
    ]

    onStateChanged:
    {
        if (state == "visible" && opacity != 0)
            opacity = 0
    }

    transitions: [
        Transition
        {
            from: "visible"
            to: "hidden"

            NumberAnimation
            {
                target: control
                property: "opacity"
                duration: 160
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
                duration:  80
            }
        }
    ]

    function showScheduleError()
    {
        showHint("", qsTr("Disabled by schedule"), "", true)
    }

    function showHint(prefix, text, iconPath, keepOpened)
    {
        if (keepOpened)
            hideTimer.stop()
        else
            hideTimer.restart()

        var prefixText = prefix ? prefix : ""
        loader.sourceComponent = textComponent
        loader.item.text = text
        loader.item.prefix = prefixText
        loader.item.textColor = ColorTheme.brightText

        activityPreloader.visible = false

        visualDataLoader.sourceComponent = imageComponent
        visualDataLoader.item.source = iconPath

        control.state = "visible"
    }

    function showCustomProcess(component, iconPath)
    {
        hideTimer.stop()

        loader.sourceComponent = component

        activityPreloader.visible = true

        visualDataLoader.sourceComponent = imageComponent
        visualDataLoader.item.source = iconPath

        control.state = "visible"
    }

    function showPreloader(text)
    {
        hideTimer.stop();

        activityPreloader.visible = false

        loader.sourceComponent = textComponent
        loader.item.text = text
        loader.item.textColor = ColorTheme.brightText
        loader.item.prefix = ""

        visualDataLoader.sourceComponent = dotsPreloader
        control.state = "visible"
    }

    function showActivity(text, iconPath)
    {
        showHint("", text, iconPath, true)
        activityPreloader.visible = true
    }

    function showSuccess(text)
    {
        showResult(text, true)
    }

    function showFailure(text)
    {
        showResult(text, false)
    }

    function showResult(text, success)
    {
        hideTimer.restart()

        var customColor = success
            ? ColorTheme.brightText
            : ColorTheme.colors.red_l2

        activityPreloader.visible = false

        loader.sourceComponent = textComponent
        loader.item.text = text
        loader.item.textColor = customColor
        loader.item.prefix = ""

        visualDataLoader.sourceComponent = imageComponent
        visualDataLoader.item.source = success
            ? "image:///skin/24x24/Outline/yes.svg?primary=light1"
            : "image:///skin/24x24/Outline/no.svg?primary=red_core"

        control.state = "visible"
    }

    function hideDelayed()
    {
        hideTimer.restart()
    }

    function hide()
    {
        hideTimer.stop()
        control.state = "hidden"
    }

    Row
    {
        id: bodyRow

        x: control.horizontalPadding
        y: control.verticalPadding
        height: 24

        Loader
        {
            id: loader
            anchors.verticalCenter: parent.verticalCenter
        }

        Item
        {
            width: visualDataLoader.hasEmptyImage ? 12 : 48
            height: 48
            anchors.verticalCenter: parent.verticalCenter

            ActivityPreloader
            {
                id: activityPreloader
                anchors.centerIn: parent
            }

            Loader
            {
                id: visualDataLoader
                anchors.centerIn: parent

                readonly property bool hasEmptyImage: sourceComponent === imageComponent
                    && (!item || !item.source.toString().length)
            }
        }
    }

    Timer
    {
        id: hideTimer

        interval: 2000
        onTriggered: control.state = "hidden"
    }

    Component
    {
        id: textComponent

        Row
        {
            property alias text: textItem.text
            property alias textColor: textItem.color
            property alias prefix: prefixItem.text

            Text
            {
                id: prefixItem

                height: 24
                wrapMode: Text.NoWrap
                verticalAlignment: Text.AlignVCenter
                color: ColorTheme.colors.light16
                font.pixelSize: 14
                font.weight: Font.Normal
                visible: text.length > 0
            }

            Text
            {
                id: textItem

                height: 24
                wrapMode: Text.NoWrap
                verticalAlignment: Text.AlignVCenter
                color: ColorTheme.brightText
                font.pixelSize: 14
                font.weight: Font.Normal
            }
        }

    }

    Component
    {
        id: imageComponent

        Image {}
    }

    Component
    {
        id: dotsPreloader

        NxDotPreloader
        {
            color: ColorTheme.brightText
            spacing: 3
            dotRadius: 4
        }
    }
}
