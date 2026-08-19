// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Nx.Mobile.Controls
import Nx.Core

ToolTip
{
    id: control

    property string title: ""
    property string description: ""
    property int stepIndex: 0
    property int stepCount: 1
    readonly property bool lastStep: stepIndex >= stepCount - 1

    signal nextClicked()
    signal skipClicked()
    signal doneClicked()

    width: 270
    padding: 20
    radius: 8
    color: ColorTheme.colors.dark13
    arrowSize: Qt.size(18, 10)

    modal: true
    closePolicy: ToolTip.NoAutoClose

    contentItem: ColumnLayout
    {
        spacing: 0
        clip: true

        Text
        {
            Layout.fillWidth: true

            text: control.title
            color: ColorTheme.colors.light1
            font.pixelSize: 18
            font.weight: Font.Medium
            lineHeightMode: Text.FixedHeight
            lineHeight: 21
            elide: Text.ElideRight
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }

        Text
        {
            Layout.fillWidth: true
            Layout.topMargin: 12

            text: control.description
            color: ColorTheme.colors.light1
            font.pixelSize: 16
            lineHeightMode: Text.FixedHeight
            lineHeight: 24
            wrapMode: Text.Wrap
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }

        RowLayout
        {
            Layout.topMargin: 20

            visible: !control.lastStep

            TextButton
            {
                Layout.alignment: Qt.AlignCenter

                text: qsTr("Skip")
                textColor: ColorTheme.colors.light1
                font.pixelSize: 18
                leftPadding: 0
                rightPadding: 0

                onClicked: control.skipClicked()
            }

            Text
            {
                Layout.fillWidth: true
                Layout.fillHeight: true

                text: `${control.stepIndex + 1}/${control.stepCount}`
                color: ColorTheme.colors.light10
                font.pixelSize: 16
                font.weight: Font.Medium
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
            }

            Button
            {
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48

                icon.source: "image://skin/24x24/Outline/arrow_right_2px.svg"
                icon.width: 24
                icon.height: 24
                radius: width / 2
                type: Button.Type.Brand

                onClicked: control.nextClicked()
            }
        }

        Button
        {
            Layout.topMargin: 20
            Layout.fillWidth: true

            text: qsTr("Ok, I got it")
            type: Button.Type.Brand
            visible: control.lastStep

            onClicked: control.doneClicked()
        }
    }

    Overlay.modal: Rectangle
    {
        color: ColorTheme.transparent(ColorTheme.colors.dark1, 0.7)
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    ShaderEffectSource
    {
        id: highlight

        parent: control.background

        sourceItem: control.target
        x: control.targetRect.x
        y: control.targetRect.y
        width: control.targetRect.width
        height: control.targetRect.height
    }
}
