// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

Control
{
    id: control

    padding: 24

    background: null

    signal closeRequested()

    contentItem: ScaleAdjuster
    {
        contentItem: Item
        {
            implicitWidth: 272
            implicitHeight: Math.max(240, pages.implicitHeight)

            PromoPages
            {
                id: pages

                anchors.fill: parent

                onCloseRequested:
                    control.closeRequested()

                PromoPage
                {
                    title: qsTr("Introducing you\nnew PTZ controls")
                    text: qsTr("Here is a quick presentation\nof what has changed")
                    verticalAlignment: Qt.AlignVCenter
                }

                PromoPage
                {
                    imageUrl: "image://svg/skin/promo/ptz_promo_drag.svg"
                    text: qsTr("Drag over any part of the video\nto activate PTZ")
                }

                PromoPage
                {
                    imageUrl: "image://svg/skin/promo/ptz_promo_keys.svg"
                    text: qsTr("Press arrows keys to move and plus or minus keys to zoom")
                }

                PromoPage
                {
                    imageUrl: "image://svg/skin/promo/ptz_promo_scroll.svg"
                    text: qsTr("Use the mouse wheel to zoom")
                }

                PromoPage
                {
                    imageUrl: "image://svg/skin/promo/ptz_promo_advanced.svg"
                    text: qsTr("Click, double-click, or drag the mouse pointer while pressing "
                        + "the Shift key to use Advanced PTZ")
                }

                PromoPage
                {
                    imageUrl: "image://svg/skin/promo/ptz_promo_old.svg"
                    text: qsTr("Go to Local Settings to enable a drag marker "
                        + "over the center of the video")
                }
            }

            MouseArea
            {
                id: backgroundMouseBlocker

                z: -1
                anchors.fill: pages
                acceptedButtons: Qt.AllButtons
                onPressed: mouse.accepted = true
                onReleased: mouse.accepted = true

                CursorOverride.shape: Qt.ArrowCursor
                CursorOverride.active: pages.hovered
            }
        }
    }
}
