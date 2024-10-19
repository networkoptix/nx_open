// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Ui

import nx.vms.client.core

Pane
{
    id: cloudPanel

    clip: true

    readonly property string login: cloudStatusWatcher.cloudLogin

    background: Rectangle
    {
        color: ColorTheme.colors.dark9

        Rectangle
        {
            width: parent.width
            height: 1
            anchors.bottom: parent.bottom
            color: ColorTheme.colors.dark7
        }

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            rippleSize: 160
        }
    }

    implicitWidth: parent ? parent.width : contentItem.implicitWidth
    implicitHeight: 48
    leftPadding: 12
    rightPadding: 12

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        onClicked:
        {
            var showSummary = cloudStatusWatcher.status == CloudStatusWatcher.Online
                || cloudStatusWatcher.status == CloudStatusWatcher.Offline

            if (showSummary)
                Workflow.openCloudSummaryScreen()
            else
                Workflow.openCloudLoginScreen()
        }
    }

    contentItem: Item
    {
        Image
        {
            id: cloudStatusImage

            source: login ? lp("/images/cloud_logged_in.png")
                          : lp("/images/cloud_not_logged_in.png")
            anchors.verticalCenter: parent.verticalCenter
        }

        Text
        {
            x: cloudStatusImage.x + cloudStatusImage.width + 12
            width: parent.width - x
            text: login
                ? login
                : qsTr("Log in to %1", "%1 is the short cloud name (like 'Cloud')").arg(applicationInfo.cloudName())
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            color: ColorTheme.colors.light10
        }
    }
}
