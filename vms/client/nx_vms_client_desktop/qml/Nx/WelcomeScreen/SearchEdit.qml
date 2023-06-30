// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.0

import Nx.Core 1.0
import Nx.Controls 1.0

TextField
{
    id: control

    property alias delay: timer.interval
    readonly property alias query: timer.query

    readonly property bool active:
    {
        const result = control.focus || control.text
        behaviorTimer.stop()
        if (result)
            xAnimation.enabled = result
        else
            behaviorTimer.start()
        return result
    }

    implicitWidth: 280
    implicitHeight: 28

    leftPadding: searchIcon.width + 2
    rightPadding: clearButton.width + 1

    onTextChanged: timer.restart()

    background: Rectangle
    {
        anchors.fill: parent

        color: "transparent"
        border.color: control.hovered ? ColorTheme.mid : ColorTheme.dark
    }

    Item
    {
        id: searchIcon

        width: height
        height: parent.height

        Image
        {
            anchors.centerIn: parent
            width: 20
            height: 20

            source: "image://svg/skin/welcome_screen/search.svg"
            sourceSize: Qt.size(width, height)
        }
    }

    Text
    {
        id: textLabel

        readonly property int _animationTimeout: 200

        x: active ? control.leftPadding + 2 : (parent.width - width) / 2

        anchors.verticalCenter: parent.verticalCenter

        font.pixelSize: 14
        color: ColorTheme.windowText

        visible: !control.displayText

        text: qsTr("Search")

        Behavior on x
        {
            id: xAnimation
            NumberAnimation { duration: textLabel._animationTimeout; easing.type: Easing.OutCubic }
        }

        Timer
        {
            id: behaviorTimer

            interval: textLabel._animationTimeout
            onTriggered: xAnimation.enabled = false
        }
    }

    Button
    {
        id: clearButton

        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 1

        height: parent.height - 2
        width: visible ? height : 0

        flat: true
        backgroundColor: ColorTheme.colors.dark5

        iconUrl: "image://svg/skin/welcome_screen/cross.svg"
        iconWidth: 20
        iconHeight: 20

        visible: control.text

        onClicked:
        {
            control.forceActiveFocus()
            control.clear()
        }
    }

    Timer
    {
        id: timer

        property string query

        onTriggered: query = control.text
    }
}
