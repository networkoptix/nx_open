// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Mobile
import Nx.Ui

import nx.vms.client.core

Control
{
    id: control

    property var uuid
    property bool isAnalyticsItem: true
    property QnCameraListModel camerasModel: null
    property EventSearchModel eventsModel: null
    property int currentEventIndex: -1
    property alias previewId: preview.previewId
    property alias previewAspectRatio: preview.previewAspectRatio
    property string trackId //< This property is used in GUI tests, do not remove.
    property alias previewState: preview.previewState
    property var resource
    property var timestampMs
    property alias title: titleItem.text
    property alias extraText: extraTextItem.text
    property alias eventTimestampText: eventTimeItem.text
    property bool shared: false
    property bool selected: false

    signal clicked()

    implicitWidth: (parent && parent.width) ?? 0
    padding: LayoutController.isTablet ? 20 : 12
    clip: true

    background: Rectangle
    {
        radius: 6
        color: control.selected ? ColorTheme.colors.dark6 : ColorTheme.colors.dark8
        border.width: 1
        border.color: control.selected ? ColorTheme.colors.brand : color
    }

    contentItem: RowLayout
    {
        spacing: LayoutController.isTablet ? 20 : 12

        Preview
        {
            id: preview

            minimumAspectRatio: LayoutController.isTablet
                ? 16.0 / 9.0
                : 1.0

            Layout.preferredWidth: LayoutController.isTablet ? 300 : 120
            Layout.alignment: Qt.AlignTop

            backgroundColor: ColorTheme.colors.dark6
            borderColor: backgroundColor
            radius: 4

            Rectangle
            {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: LayoutController.isTablet ? 4 : 2

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
            spacing: LayoutController.isTablet ? 12 : 4

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop | Qt.AlignLeft

            Text
            {
                id: titleItem

                width: parent.width
                visible: !!text
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: LayoutController.isTablet ? 3 : 2
                verticalAlignment: Qt.AlignVCenter

                color: ColorTheme.colors.light4
                font.pixelSize: LayoutController.isTablet ? 18 : 16
                font.weight: Font.Medium
                lineHeight: 1.25
            }

            Text
            {
                id: extraTextItem

                width: parent.width
                visible: !!NxGlobals.toPlainText(text)
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                verticalAlignment: Qt.AlignVCenter

                color: ColorTheme.colors.light10
                font.pixelSize: LayoutController.isTablet ? 16 : 14
                font.weight: Font.Normal
                lineHeight: 1.25
            }

            Text
            {
                id: cameraName

                width: parent.width
                visible: !!text
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 1
                verticalAlignment: Qt.AlignVCenter

                color: ColorTheme.colors.light10
                font.pixelSize: LayoutController.isTablet ? 16 : 14
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
                wrapMode: Text.Wrap
                maximumLineCount: 1
                verticalAlignment: Qt.AlignVCenter

                color: ColorTheme.colors.light16
                font.pixelSize: 14
                font.weight: LayoutController.isTablet ? Font.Medium : Font.Normal
                lineHeight: 1.25

                text:
                {
                    return EventSearchUtils.timestampText(
                        control.timestampMs,
                        windowContext.mainSystemContext,
                        /*alwaysShowDate*/ true)
                }
            }
        }
    }

    TapHandler
    {
        onTapped: control.clicked()
    }
}
