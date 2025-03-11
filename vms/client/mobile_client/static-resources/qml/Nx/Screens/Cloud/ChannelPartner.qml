// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Ui

Page
{
    id: channelPartnerScreen
    objectName: "channelPartnerScreen"

    title: qsTr("Channel Partner")

    padding: 0

    property var profileWatcher

    onLeftButtonClicked: Workflow.popCurrentScreen()

    Column
    {
        width: parent.width
        spacing: 4

        Repeater
        {
            model: profileWatcher.channelPartnerList

            delegate: Rectangle
            {
                id: channelPartnerItem

                readonly property bool selected:
                    modelData.id == profileWatcher.channelPartner

                width: parent.width
                height: 56

                color: ColorTheme.colors.dark6

                Text
                {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name
                    font.pixelSize: 16
                    font.weight: Font.Normal
                    color: channelPartnerItem.selected
                        ? ColorTheme.colors.brand_core
                        : ColorTheme.colors.light4
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.right: parent.right
                    anchors.rightMargin: image.anchors.rightMargin + image.sourceSize.width
                    elide: Text.ElideRight
                }

                ColoredImage
                {
                    id: image

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 20

                    visible: channelPartnerItem.selected
                    sourceSize: Qt.size(24, 24)
                    primaryColor: ColorTheme.colors.brand_core
                    sourcePath: "image://skin/24x24/Outline/yes.svg"
                }

                TapHandler
                {
                    onSingleTapped:
                    {
                        profileWatcher.channelPartner = modelData.id
                    }
                }
            }
        }
    }
}
