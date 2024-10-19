// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core

import nx.vms.client.core

Item
{
    id: control

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    property alias description: descriptionText.text
    property bool needDigestCloudPassword: false
    readonly property bool cloudAvailable:
        cloudStatusWatcher.status === CloudStatusWatcher.Online

    Column
    {
        id: column

        width: parent.width
        spacing: 4

        Text
        {
            id: descriptionText

            width: parent.width
            font.pixelSize: 14
            elide: Text.ElideRight
            color: enabled ? ColorTheme.windowText : ColorTheme.colors.dark13
        }

        Row
        {
            x: -2
            spacing: 4

            Image
            {
                id: cloudImage
                source:
                {
                    if (!cloudAvailable)
                        return "/images/cloud_unavailable.png"

                    return enabled
                        ? "/images/cloud.png"
                        : "/images/cloud_disabled.png"
                }
            }

            Item
            {
                width: cloudImage.width
                height: cloudImage.height

                Image
                {
                    source: "/images/cloud_locked.svg"
                    sourceSize: Qt.size(20, 20)
                    visible: enabled && control.needDigestCloudPassword
                    anchors.centerIn: parent
                }
            }
        }
    }
}
