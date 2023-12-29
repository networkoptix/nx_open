// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml.Models

import Nx
import Nx.Controls
import Nx.Core
import nx.vms.client.core
import nx.vms.client.desktop

Control
{
    id: control

    background: null

    property bool currentShowBasics: false
    property bool currentShowTargetLock: false

    signal closeRequested()

    function setPagesVisibility(showBasics: bool, showTargetLock: bool)
    {
        if (currentShowBasics == showBasics && currentShowTargetLock == showTargetLock)
            return

        currentShowBasics = showBasics
        currentShowTargetLock = showTargetLock

        pagesRepeater.model = null

        pagesModel.clear()

        pagesModel.append({
            "title": qsTr("Introducing\nNew PTZ controls"),
            "text": qsTr("Here is a quick guide\non what has changed."),
            "imageUrl": "",
        })

        if (showBasics)
        {
            pagesModel.append({
                "title": "",
                "text": qsTr("Drag over any part of the video\nto activate PTZ"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_drag.svg"
            })

            pagesModel.append({
                "title": "",
                "text": qsTr("Press arrow keys to move around. Use plus and minus keys to zoom."),
                "imageUrl": "image://svg/skin/promo/ptz_promo_keys.svg"
            })

            pagesModel.append({
                "title": "",
                "text": qsTr("Use mouse wheel to zoom"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_scroll.svg"
            })

            pagesModel.append({
                "title": "",
                "text": qsTr("Click, double-click, or drag\nmouse pointer while pressing\nShift "
                    + "key to use Advanced PTZ"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_advanced.svg"
            })

            pagesModel.append({
                "title": "",
                "text": qsTr("Go to Local Settings to enable a drag marker over "
                    + "the center of the video"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_old.svg"
            })
        }

        if (showTargetLock)
        {
            pagesModel.append({
                "title": "",
                "text": qsTr("To use Target Lock Tracking,\nPress Alt + Click to follow object"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_tracking.svg"
            })
        }

        pagesModel.append({
            "title": "",
            "text": qsTr("You can enable this guide again by going to "
                + "Local Settings > Advanced > Reset All Warnings"),
            "imageUrl": "image://svg/skin/promo/ptz_promo_show_again.svg"
        })

        pagesRepeater.model = pagesModel
    }

    ListModel
    {
        id: pagesModel
    }

    contentItem: ScaleAdjuster
    {
        contentItem: Item
        {
            implicitWidth: 280
            implicitHeight: 240

            PromoPages
            {
                id: pages

                anchors.fill: parent

                onCloseRequested: control.closeRequested()

                Repeater
                {
                    id: pagesRepeater

                    model: pagesModel

                    Loader
                    {
                        active: SwipeView.isCurrentItem || SwipeView.isNextItem || SwipeView.isPreviousItem

                        sourceComponent: PromoPage
                        {
                            imageUrl: model.imageUrl
                            title: model.title
                            text: model.text
                            verticalAlignment: model.imageUrl ? Qt.AlignVCenter : Qt.AlignTop
                        }
                    }
                }
            }
        }
    }
}
