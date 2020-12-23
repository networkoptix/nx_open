import QtQuick 2.11
import QtQuick.Controls 2.4

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
                    imageUrl: "qrc:///skin/promo/ptz_promo_drag.png"
                    text: qsTr("Drag over any part of the video\nto activate PTZ")
                }

                PromoPage
                {
                    imageUrl: "qrc:///skin/promo/ptz_promo_keys.png"
                    text: qsTr("Press arrows keys to move and plus or minus keys to zoom")
                }

                PromoPage
                {
                    imageUrl: "qrc:///skin/promo/ptz_promo_scroll.png"
                    text: qsTr("Use the mouse wheel to zoom")
                }

                PromoPage
                {
                    imageUrl: "qrc:///skin/promo/ptz_promo_advanced.png"
                    text: qsTr("Click, double-click, or drag the mouse pointer while pressing "
                        + "the Shift key to use Advanced PTZ")
                }

                PromoPage
                {
                    imageUrl: "qrc:///skin/promo/ptz_promo_old.png"
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
