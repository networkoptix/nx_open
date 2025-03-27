// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.mobile

Flow
{
    id: control

    spacing: 6

    readonly property bool hasTags: repeater.count > 0
    property alias model: repeater.model

    Repeater
    {
        id: repeater

        delegate: Rectangle
        {
            height: 36
            width: dataRow.width

            color: "transparent"
            border.color: ColorTheme.colors.dark15
            radius: 6

            Row
            {
                id: dataRow

                spacing: 4

                anchors.centerIn: parent

                leftPadding: 12
                rightPadding: 12

                ColoredImage
                {
                    id: objectBasedIcon

                    visible: tagText.text === BookmarkConstants.objectBasedTagName
                    anchors.verticalCenter: parent.verticalCenter
                    sourcePath: "image://skin/20x20/Solid/camera.svg?primary=light4"
                    sourceSize: Qt.size(20, 20)
                }

                Text
                {
                    id: tagText

                    width:
                    {
                        const iconWidth = objectBasedIcon.visible
                            ? objectBasedIcon.width + dataRow.spacing
                            : 0
                        const maxWidth = control.parent.width
                            - dataRow.leftPadding
                            - dataRow.rightPadding
                            - iconWidth
                        return  Math.min(implicitWidth, maxWidth)
                    }
                    anchors.verticalCenter: parent.verticalCenter
                    text: model.modelData
                    font.pixelSize: 14
                    font.weight: 400
                    color: ColorTheme.colors.light4
                    elide: Text.ElideRight
                }
            }
        }
    }
}
