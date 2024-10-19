// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0
import Nx.Controls 1.0

Item
{
    property int checkState: Qt.Unchecked
    property color uncheckedColor: ColorTheme.colors.dark13
    property color uncheckedIndicatorColor: ColorTheme.colors.dark17
    property color checkedColor: ColorTheme.colors.green_core
    property color checkedIndicatorColor: ColorTheme.colors.green_l2
    property color handleColor: ColorTheme.colors.dark5
    property color preloaderColor: ColorTheme.colors.dark17

    property int animationDuration: 150

    implicitWidth: 38
    implicitHeight: 20

    layer.enabled: !enabled
    opacity: enabled ? 1.0 : 0.3

    function toggle()
    {
        checkState = checkState == Qt.Unchecked
            ? Qt.Checked
            : Qt.Unchecked
    }

    Rectangle
    {
        id: uncheckedLayer

        anchors.fill: parent

        radius: height / 2
        color: uncheckedColor

        Rectangle
        {
            id: uncheckedindicator

            width: parent.height / 2 + 2
            height: width
            radius: height / 2
            x: parent.width - parent.height / 2 - width / 2
            anchors.verticalCenter: parent.verticalCenter
            color: uncheckedIndicatorColor
            visible: checkState != Qt.PartiallyChecked

            Rectangle
            {
                width: parent.width - 4
                height: width
                radius: height / 2
                anchors.centerIn: parent
                color: uncheckedColor
            }
        }

        Rectangle
        {
            id: checkedLayer

            anchors.fill: parent
            radius: height / 2
            color: checkedColor

            Rectangle
            {
                id: checkedIndicator

                width: 2
                height: parent.height / 2
                x: parent.height / 2
                anchors.verticalCenter: parent.verticalCenter
                color: checkedIndicatorColor
                visible: checkState != Qt.PartiallyChecked
            }

            opacity:
            {
                const stableState = checkState == Qt.PartiallyChecked
                    ? d.lastStableCheckState
                    : checkState

                return stableState == Qt.Unchecked ? 0.0 : 1.0
            }

            Behavior on opacity
            {
                NumberAnimation
                {
                    duration: checkState == Qt.PartiallyChecked
                        ? animationDuration / 2
                        : animationDuration
                }
            }
        }

        Rectangle
        {
            id: handle

            width: parent.height - 4
            height: width
            radius: height / 2
            color: handleColor
            anchors.verticalCenter: parent.verticalCenter

            x:
            {
                switch (checkState)
                {
                    case Qt.Checked:
                        return parent.width - width - 2
                    case Qt.PartiallyChecked:
                        return (parent.width - width) / 2
                    default:
                        return 2
                }
            }

            Behavior on x { NumberAnimation { duration: animationDuration } }

            CircleBusyIndicator
            {
                x: 2
                y: 2
                width: parent.width - 2 * x
                height: parent.height - 2 * y
                lineWidth: 1
                color: preloaderColor
                visible: control.enabled && checkState == Qt.PartiallyChecked
            }
        }
    }

    NxObject
    {
        id: d
        property int lastStableCheckState: Qt.Unchecked
    }

    onCheckStateChanged:
    {
        if (checkState != Qt.PartiallyChecked)
            d.lastStableCheckState = checkState
    }
}
