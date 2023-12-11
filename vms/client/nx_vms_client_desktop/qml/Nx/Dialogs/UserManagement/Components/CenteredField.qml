// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

Item
{
    id: control

    width: parent.width
    height: Math.max(innerItem.childrenRect.height, labelRow.height)

    default property alias data: innerItem.data
    property alias text: label.text

    property real leftSideMargin: 240
    property real rightSideMargin: 240

    property real labelHeight: 28

    property alias hint: textHint.data
    property alias toolTipText: hintButton.toolTipText

    readonly property real labelImplicitWidth: labelRow.width

    baselineOffset: label.y + label.baselineOffset

    Item
    {
        id: innerItem

        baselineOffset: control.baselineOffset

        anchors
        {
            left: control.left
            leftMargin: control.leftSideMargin
            right: control.right
            rightMargin: control.rightSideMargin
        }
    }

    RowLayout
    {
        id: labelRow

        anchors.top: parent.top
        anchors.bottom: control.labelHeight > 0 ? undefined : parent.bottom
        anchors.right: innerItem.left
        anchors.rightMargin: 8

        spacing: 4

        height: control.labelHeight > 0 ? control.labelHeight : label.implicitHeight

        Text
        {
            id: label

            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            Layout.fillWidth: true
            Layout.fillHeight: true

            horizontalAlignment: Text.AlignRight
            verticalAlignment: Qt.AlignVCenter

            font: Qt.font({pixelSize: 14, weight: Font.Normal})
            color: ColorTheme.colors.light16
        }

        ContextHintButton
        {
            id: hintButton
            toolTipText: ""
            visible: !!toolTipText
        }

        Item
        {
            id: textHint

            Layout.alignment: Qt.AlignVCenter

            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height

            visible: children.length > 0
        }
    }
}
