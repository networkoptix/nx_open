// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Templates as T

import Nx.Core 1.0

import "private"

T.RadioButton
{
    id: control

    property color backgoundColor: ColorTheme.colors.dark6
    property color checkedBackgroundColor: ColorTheme.colors.dark6

    property alias extraText: extraTextItem.text

    implicitWidth: leftPadding + rightPadding + contentItem.width
    implicitHeight: topPadding + bottomPadding + textArea.implicitHeight

    font.pixelSize: 16

    topPadding: 16
    bottomPadding: 12
    spacing: 8
    leftPadding: 16
    rightPadding: 16
    opacity: control.enabled ? 1.0 : 0.3

    background: Rectangle
    {
        color: control.checked
            ? control.checkedBackgroundColor
            : control.backgoundColor
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

    contentItem: Item
    {
        Column
        {
            id: textArea

            spacing: 4

            width: parent.width - (indicator.width + control.spacing)
            Text
            {
                id: textItem

                width: parent.width
                text: control.text
                font: control.font
                color: ColorTheme.windowText
                wrapMode: Text.Wrap
            }

            Text
            {
                id: extraTextItem

                width: parent.width
                visible: !!text
                wrapMode: Text.Wrap
                font.pixelSize: 13
                color: ColorTheme.colors.light12
            }
        }
    }
}
