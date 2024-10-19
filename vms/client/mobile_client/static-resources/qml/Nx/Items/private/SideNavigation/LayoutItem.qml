// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Nx.Core
import Nx.Mobile

SideNavigationItem
{
    id: sideNavigationItem

    property string text
    property string resourceId
    property bool shared: false
    property int count: 0
    property int type: QnLayoutsModel.Layout

    implicitHeight: 40

    Row
    {
        anchors.fill: parent
        spacing: 12
        anchors.leftMargin: 12
        anchors.rightMargin: 16

        Image
        {
            id: icon

            anchors.verticalCenter: parent.verticalCenter
            source:
            {
                var icon = undefined

                if (type == QnLayoutsModel.AllCameras)
                {
                    return active
                        ? lp("/images/all_cameras_active.png")
                        : lp("/images/all_cameras.png")
                }

                if (type == QnLayoutsModel.Layout)
                {
                    if (shared)
                    {
                        return active
                            ? lp("/images/global_layout_active.png")
                            : lp("/images/global_layout.png")
                    }

                    return active
                        ? lp("/images/layout_active.png")
                        : lp("/images/layout.png")
                }

                return undefined
            }
        }

        Row
        {
            width: parent.width - icon.width - countLabel.width - 2 * parent.spacing
            height: parent.height
            spacing: 4

            Text
            {
                id: label

                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, parent.width)

                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                color: active ? ColorTheme.windowText : ColorTheme.colors.light10

                text: sideNavigationItem.text
            }
        }

        Text
        {
            id: countLabel

            anchors.verticalCenter: parent.verticalCenter

            text: count
            font.pixelSize: 15
            color: active ? ColorTheme.colors.light16 : ColorTheme.colors.dark16
        }
    }
}
