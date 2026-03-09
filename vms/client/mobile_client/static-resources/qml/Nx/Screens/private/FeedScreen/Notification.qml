// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile
import Nx.Ui

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
    property bool selected: false

    readonly property bool isTabletLayout:
    {
        if (windowContext.deprecatedUiController.currentScreen === Controller.SessionsScreen)
            return false //< On the SessionsScreen it is always mobile layout for the feed.

        return LayoutController.isTabletLayout
    }

    signal clicked()

    onIsTabletLayoutChanged:
    {
        if (isTabletLayout)
            expanded = false
    }

    implicitWidth: 320
    padding: 20

    background: Rectangle
    {
        color: viewed ? ColorTheme.colors.dark6 : ColorTheme.colors.dark9
        radius: 10

        border.color: ColorTheme.colors.brand
        border.width: control.selected ? 1 : 0

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
            visible: !!source
            spacing: 8

            ColoredImage
            {
                sourcePath: "image://skin/20x20/Solid/site_local.svg"
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

        Item
        {
            Layout.fillWidth: true
            Layout.preferredHeight: control.isTabletLayout
                ? landscapeContentLayout.implicitHeight
                : mobileContentLayout.implicitHeight

            ColumnLayout
            {
                id: mobileContentLayout

                anchors.fill: parent
                visible: !control.isTabletLayout
                spacing: 16

                LayoutItemProxy
                {
                    Layout.fillWidth: true

                    target: title
                }

                LayoutItemProxy
                {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180

                    target: image
                    visible: image.status != Image.Null
                }

                LayoutItemProxy
                {
                    Layout.fillWidth: true

                    target: description
                }

                LayoutItemProxy
                {
                    Layout.fillWidth: true

                    target: footerLayout
                }
            }

            RowLayout
            {
                id: landscapeContentLayout

                anchors.fill: parent
                visible: control.isTabletLayout
                spacing: 24

                ColumnLayout
                {
                    Layout.alignment: Qt.AlignTop

                    spacing: 16

                    LayoutItemProxy
                    {
                        Layout.fillWidth: true

                        target: title
                    }

                    LayoutItemProxy
                    {
                        Layout.fillWidth: true

                        target: description
                    }

                    LayoutItemProxy
                    {
                        Layout.fillWidth: true

                        target: footerLayout
                    }
                }

                LayoutItemProxy
                {
                    Layout.preferredHeight: 116
                    Layout.preferredWidth: 180
                    Layout.alignment: Qt.AlignVCenter

                    target: image
                    visible: image.status != Image.Null
                }
            }

            Text
            {
                id: title

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

                fillMode: Image.PreserveAspectCrop

                layer.enabled: true
                layer.effect: OpacityMask
                {
                    maskSource: Rectangle
                    {
                        width: image.paintedWidth || image.width
                        height: image.paintedHeight || image.height
                        radius: 4
                    }
                }

                ColoredImage
                {
                    anchors.centerIn: parent

                    visible: !control.isTabletLayout && !!url
                    sourcePath: "image://skin/48x48/Solid/play.svg"
                    sourceSize: Qt.size(48, 48)
                    primaryColor: ColorTheme.colors.light1
                }

                Rectangle
                {
                    anchors.fill: parent

                    visible: image.status != Image.Ready
                    color: ColorTheme.colors.dark8

                    Text
                    {
                        anchors.centerIn: parent

                        visible: image.status === Image.Error
                        text: qsTr("No data")
                        font.pixelSize: 12
                        color: ColorTheme.colors.dark17
                    }
                }
            }

            Text
            {
                id: description

                color: ColorTheme.colors.light10
                font.pixelSize: 16

                wrapMode: Text.Wrap
                maximumLineCount: expanded ? undefined : 2
                elide: Text.ElideRight
                textFormat: Text.StyledText
            }

            RowLayout
            {
                id: footerLayout

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
                    visible: !control.isTabletLayout && description.truncated

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
                    visible: !control.isTabletLayout && description.truncated

                    sourceSize: Qt.size(24, 24)
                    sourcePath: "image://skin/24x24/Outline/arrow_down_2px.svg"
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
}
