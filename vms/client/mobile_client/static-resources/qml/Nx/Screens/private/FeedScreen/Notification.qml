// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Nx.Core
import Nx.Core.Controls

Control
{
    id: control

    objectName: "notification"

    property alias title: title.text
    property alias description: description.text
    property alias time: time.text
    property alias image: image.source
    property string source: ""
    property bool viewed: false
    property string url: ""
    property bool expanded: false

    readonly property int kTitleRightPadding: 24

    signal clicked()

    implicitWidth: 320
    padding: 20

    background: Rectangle
    {
        color: viewed ? ColorTheme.colors.dark6 : ColorTheme.colors.dark9
        radius: 10

        Rectangle
        {
            id: indicator

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 28
            anchors.rightMargin: 20

            opacity: viewed ? 0.0 : 1.0
            width: 8
            height: 8
            radius: width / 2
            color: ColorTheme.colors.brand

            Behavior on opacity
            {
                NumberAnimation
                {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
        }

        MouseArea
        {
            anchors.fill: parent
            onClicked: control.clicked()
        }

        Behavior on color
        {
            ColorAnimation
            {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
    }

    contentItem: ColumnLayout
    {
        spacing: 20

        RowLayout
        {
            Layout.rightMargin: kTitleRightPadding

            visible: !!source
            spacing: 8

            ColoredImage
            {
                sourcePath: "image://skin/20x20/Solid/system_local.svg"
                sourceSize: Qt.size(20, 20)
                primaryColor: ColorTheme.colors.light10
            }

            Text
            {
                Layout.fillWidth: true

                text: source
                visible: !!text
                font.pixelSize: 16
                color: ColorTheme.colors.light10

                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        ColumnLayout
        {
            spacing: 16

            Text
            {
                id: title

                Layout.fillWidth: true
                Layout.rightMargin: source ? undefined : kTitleRightPadding

                font.pixelSize: 18
                font.weight: Font.Medium
                color: ColorTheme.colors.light4

                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                textFormat: Text.StyledText
            }

            Image
            {
                id: image

                Layout.fillWidth: true
                Layout.preferredHeight: 180

                visible: status === Image.Ready

                fillMode: Image.PreserveAspectCrop

                layer.enabled: true
                layer.effect: OpacityMask
                {
                    maskSource: Rectangle
                    {
                        width: image.paintedWidth
                        height: image.paintedHeight
                        radius: 4
                    }
                }

                ColoredImage
                {
                    anchors.centerIn: parent

                    visible: !!url
                    sourcePath: "image://skin/feed/play.svg"
                    sourceSize: Qt.size(48, 48)
                    primaryColor: ColorTheme.colors.light1
                }
            }

            Text
            {
                id: description

                Layout.fillWidth: true

                color: ColorTheme.colors.light10
                font.pixelSize: 16

                wrapMode: Text.Wrap
                maximumLineCount: expanded ? undefined : 2
                elide: Text.ElideRight
                textFormat: Text.StyledText
            }
        }

        RowLayout
        {
            spacing: 4

            Text
            {
                id: time

                Layout.fillWidth: true

                font.pixelSize: 14
                font.capitalization: Font.AllUppercase
                font.weight: Font.Medium
                color: ColorTheme.colors.light16
            }

            Text
            {
                visible: description.truncated

                text: qsTr("Show more")
                font.pixelSize: 16
                font.capitalization: Font.AllUppercase
                font.weight: Font.Medium
                color: ColorTheme.colors.light16

                MouseArea
                {
                    anchors.fill: parent
                    anchors.margins: -20

                    onClicked: expanded = true
                }
            }

            ColoredImage
            {
                visible: description.truncated

                sourceSize: Qt.size(24, 24)
                sourcePath: "image://skin/feed/arrow_down_2px.svg"
                primaryColor: ColorTheme.colors.light16

                MouseArea
                {
                    anchors.fill: parent
                    anchors.margins: -20

                    onClicked: expanded = true
                }
            }
        }
    }
}
