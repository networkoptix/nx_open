// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import Qt5Compat.GraphicalEffects
import Nx 1.0
import Nx.Core 1.0

Item
{
    property int checkState: Qt.Unchecked

    property bool hovered: false

    property color uncheckedColor: ColorTheme.midlight
    property color uncheckedIndicatorColor: ColorTheme.lighter(ColorTheme.midlight, 4)
    property color checkedColor: ColorTheme.colors.green_core
    property color checkedIndicatorColor: ColorTheme.colors.green_l2
    property color handleColor: "transparent"

    property int animationDuration: 0

    implicitWidth: 33
    implicitHeight: 17
    baselineOffset: implicitHeight - 2

    readonly property bool _transparentHandle: handleColor.a === 0

    function _hoverColor(color)
    {
        return ColorTheme.lighter(color, hovered ? 1 : 0)
    }

    Rectangle
    {
        id: uncheckedLayer

        visible: !_transparentHandle
        anchors.fill: parent

        radius: height / 2
        color: _hoverColor(uncheckedColor)

        Rectangle
        {
            id: uncheckedindicator

            width: parent.height / 2 + 2.5
            height: width
            radius: height / 2
            x: parent.width - parent.height / 2 - width / 2
            anchors.verticalCenter: uncheckedLayer.verticalCenter
            color: _hoverColor(uncheckedIndicatorColor)

            Rectangle
            {
                width: parent.width - 4
                height: width
                radius: height / 2
                anchors.centerIn: parent
                color: _hoverColor(uncheckedColor)
            }

            visible: checkState !== Qt.PartiallyChecked
        }

        Rectangle
        {
            id: checkedLayer

            anchors.fill: parent
            radius: height / 2
            color: _hoverColor(checkedColor)

            Rectangle
            {
                id: checkedIndicator

                width: 2
                height: parent.height / 2
                x: parent.height / 2
                anchors.verticalCenter: checkedLayer.verticalCenter
                color: _hoverColor(checkedIndicatorColor)
                visible: checkState !== Qt.PartiallyChecked
            }

            opacity:
            {
                if (checkState === Qt.Unchecked)
                    return 0
                if (checkState === Qt.Checked)
                    return 1
                return 0.4
            }
            Behavior on opacity { NumberAnimation { duration: animationDuration } }
        }
    }

    Item
    {
        id: opacityMaskSource

        anchors.fill: parent
        visible: !_transparentHandle

        Rectangle
        {
            id: handle

            width: parent.height - 2
            height: width
            radius: height / 2
            color: _transparentHandle ? "white" : _hoverColor(handleColor)
            anchors.verticalCenter: opacityMaskSource.verticalCenter

            x:
            {
                if (checkState === Qt.Unchecked)
                    return 1
                if (checkState === Qt.Checked)
                    return parent.width - width - 1
                return (parent.width - width) / 2
            }
            Behavior on x { NumberAnimation { duration: animationDuration } }
        }
    }

    OpacityMask
    {
        visible: _transparentHandle
        anchors.fill: parent
        source: uncheckedLayer
        maskSource: opacityMaskSource
        opacity: enabled ? 1.0 : 0.3
        invert: true
    }
}
