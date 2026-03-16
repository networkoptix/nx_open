// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Nx.Core
import Nx.Core.Items
import Nx.Core.Controls
import Nx.Mobile
import Nx.Ui

import nx.vms.client.core

Control
{
    id: control

    property bool isAnalyticsItem: true
    property QnCameraListModel camerasModel: null
    property EventSearchModel eventsModel: null
    property int currentEventIndex: -1
    property alias previewId: preview.previewId
    property string trackId //< This property is used in GUI tests, do not remove.
    property alias previewState: preview.previewState
    property var resource
    property var timestampMs
    property alias title: titleItem.text
    property alias eventTimestampText: eventTimeItem.text
    property bool shared: false

    implicitWidth: (parent && parent.width) ?? 0
    implicitHeight: 122
    padding: 12
    clip: true

    background: Rectangle
    {
        radius: 6
        color: control.selected ? ColorTheme.colors.dark6 : ColorTheme.colors.dark8
        border.width: 1
        border.color:control.selected ? ColorTheme.colors.brand : color
    }

    contentItem: RowLayout
    {
        spacing: 12

        Preview
        {
            id: preview

            Layout.preferredWidth: 120
            Layout.fillHeight: true

            backgroundColor: ColorTheme.colors.dark6
            borderColor: backgroundColor

            layer.enabled: true
            layer.effect: OpacityMask
            {
                maskSource: Rectangle
                {
                    width: preview.width
                    height: preview.height
                    radius: 4
                }
            }

            Rectangle
            {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 2

                radius: 4
                width: 48
                height: 48
                color: ColorTheme.transparent(ColorTheme.colors.dark3, 0.6)
                visible: control.shared

                ColoredImage
                {
                    anchors.centerIn: parent

                    sourcePath: "image://skin/20x20/Solid/shared.svg?primary=light10&secondary=green"
                    sourceSize: Qt.size(24, 24)
                }
            }
        }

        Column
        {
            spacing: 4

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop | Qt.AlignLeft

            Text
            {
                id: titleItem

                width: parent.width
                visible: !!text
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2

                color: ColorTheme.colors.light4
                font.pixelSize: 16
                font.weight: Font.Medium
                lineHeight: 1.25
            }

            Text
            {
                id: cameraName

                width: parent.width
                visible: !!text
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 1

                color: ColorTheme.colors.light10
                font.pixelSize: 14
                font.weight: Font.Normal
                lineHeight: 1.25

                text: control.resource.name
            }

            Text
            {
                id: eventTimeItem

                width: parent.width
                visible: text.length
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 1

                color: ColorTheme.colors.light16
                font.pixelSize: 14
                font.weight: Font.Normal
                lineHeight: 1.25

                text:
                {
                    return EventSearchUtils.timestampText(
                        control.timestampMs, windowContext.mainSystemContext)
                }
            }
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: Workflow.openEventDetailsScreen(
            camerasModel, eventsModel, currentEventIndex, isAnalyticsItem)
    }
}
