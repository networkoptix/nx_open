// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Nx.Core
import Nx.Controls
import Nx.Core.Items
import Nx.Core.Controls 
import Nx.Items

import nx.vms.api

Control
{
    id: cameraItem

    property alias text: label.text
    property alias thumbnail: thumbnail.source
    property int status

    signal clicked
    signal pressAndHold

    implicitWidth: 200
    implicitHeight: 48

    QtObject
    {
        id: d

        property bool offline: status == API.ResourceStatus.offline ||
                               status == API.ResourceStatus.undefined ||
                               status == API.ResourceStatus.unauthorized
        property bool unauthorized: status == API.ResourceStatus.unauthorized
    }

    contentItem: Row
    {
        width: parent.availableWidth
        height: parent.availableHeight

        Rectangle
        {
            id: thumbnailContainer

            width: 80
            height: parent.height
            color: d.offline ? ColorTheme.colors.dark7 : ColorTheme.colors.dark4

            Loader
            {
                id: thumbnailContentLoader

                anchors.centerIn: parent
                sourceComponent:
                {
                    if (d.offline || d.unauthorized)
                        return thumbnailDummyComponent
                    else if (!cameraItem.thumbnail)
                        return thumbnailPreloaderComponent
                    else
                        return thumbnailComponent
                }
            }

            Image
            {
                anchors.centerIn: parent
                source: d.unauthorized ? lp("/images/camera_locked.png") : lp("/images/camera_offline.png")
                visible: d.offline
                sourceSize.width: 40
                sourceSize.height: 40
            }

            Image
            {
                id: thumbnail
                anchors.fill: parent
                fillMode: Qt.KeepAspectRatio
                asynchronous: true
                visible: !d.offline && status == Image.Ready
            }
        }

        Item
        {
            id: spacer
            width: 16
            height: 1
        }

        StatusIndicator
        {
            id: statusIndicator
            status: cameraItem.status
            anchors.verticalCenter: parent.verticalCenter
        }

        Text
        {
            id: label
            leftPadding: 6
            width: parent.width - statusIndicator.width - spacer.width - thumbnailContainer.width
            height: 48
            verticalAlignment: Text.AlignVCenter
            maximumLineCount: 1
            wrapMode: Text.WordWrap
            font.pixelSize: 16
            font.weight: d.offline ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            color: d.offline ? ColorTheme.colors.dark11 : ColorTheme.windowText
        }
    }

    Component
    {
        id: thumbnailDummyComponent

        Image
        {
            source: d.unauthorized ? lp("/images/camera_locked.png") : lp("/images/camera_offline.png")
            sourceSize.width: 40
            sourceSize.height: 40
        }
    }

    Component
    {
        id: thumbnailPreloaderComponent

        NxDotPreloader
        {
            scale: 0.5
        }
    }

    Component
    {
        id: thumbnailComponent

        Image
        {
            source: cameraItem.thumbnail
            width: thumbnailContainer.width
            height: thumbnailContainer.height
            fillMode: Qt.KeepAspectRatio
        }
    }

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        onClicked: cameraItem.clicked()
    }

    MaterialEffect
    {
        clip: true
        anchors.fill: parent
        mouseArea: mouseArea
        rippleSize: 160
    }
}
