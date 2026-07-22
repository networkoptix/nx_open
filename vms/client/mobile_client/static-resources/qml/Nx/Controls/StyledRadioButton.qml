// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Templates as T

import Nx.Core

import "private"

T.RadioButton
{
    id: control

    property int backgroundRadius: 0
    property color backgroundColor: ColorTheme.colors.dark6
    property color checkedBackgroundColor: ColorTheme.colors.dark8
    property color backgroundBorderColor: "transparent"
    property int backgroundBorderWidth: 0
    property color textColor: ColorTheme.colors.light10
    property color checkedTextColor: ColorTheme.colors.brand_core

    property alias extraText: extraTextItem.text

    implicitWidth: leftPadding + rightPadding + contentItem.width
    implicitHeight: topPadding
        + bottomPadding
        + Math.max(textArea.implicitHeight, icon.status === Image.Ready ? 24 : 0)

    font.pixelSize: 16

    topPadding: 16
    bottomPadding: 16
    spacing: 8
    leftPadding: 16
    rightPadding: 16
    opacity: control.enabled ? 1.0 : 0.3

    background: Rectangle
    {
        radius: control.backgroundRadius
        color: control.checked
            ? control.checkedBackgroundColor
            : control.backgroundColor
        border.width: control.backgroundBorderWidth
        border.color: control.backgroundBorderColor

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            rippleSize: 160
            mouseArea: control
        }
    }

    indicator: CheckIndicator
    {
        anchors.right: parent.right
        anchors.rightMargin: control.rightPadding
        anchors.verticalCenter: parent.verticalCenter
        checked: control.checked
        color: "transparent"
        checkColor: ColorTheme.colors.brand_core
        checkedColor: "transparent"
        lineWidth: 1.5
        checkMarkScale: 1.2
    }

    contentItem: RowLayout
    {
        Image
        {
            id: icon

            Layout.preferredWidth: 24
            Layout.preferredHeight: 24

            source: control.icon.source
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
            visible: status === Image.Ready
        }

        Column
        {
            id: textArea

            Layout.fillWidth: true
            Layout.rightMargin: indicator ? (indicator.width + control.spacing) : 0

            spacing: 4

            Text
            {
                id: textItem

                width: parent.width
                text: control.text
                font: control.font
                color: control.checked ? control.checkedTextColor : control.textColor
                wrapMode: Text.Wrap
            }

            Text
            {
                id: extraTextItem

                width: parent.width
                visible: !!text
                wrapMode: Text.Wrap
                font.pixelSize: 14
                color: control.checked ? control.checkedTextColor : ColorTheme.colors.light16
            }
        }
    }
}
