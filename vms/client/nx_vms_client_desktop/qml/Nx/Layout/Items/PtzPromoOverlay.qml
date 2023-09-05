// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml.Models

import Nx.Controls

import nx.vms.client.desktop

Control
{
    id: control

    padding: 24

    background: null

    signal closeRequested()

    function setPagesVisibility(showBasics: bool, showTargetLock: bool)
    {
        pagesRepeater.model = null

        pagesModel.clear()

        pagesModel.append({
            "title": qsTr("Introducing you\nnew PTZ controls"),
            "text": qsTr("Here is a quick presentation\nof what has changed"),
            "imageUrl": ""
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
                "text": qsTr("Press arrows keys to move and plus or minus keys to zoom"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_keys.svg"
            })

            pagesModel.append({
                "title": "",
                "text": qsTr("Use the mouse wheel to zoom"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_scroll.svg"
            })

            pagesModel.append({
                "title": "",
                "text": qsTr("Click, double-click, or drag the mouse pointer while pressing "
                    + "the Shift key to use Advanced PTZ"),
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
                "title": qsTr("Target lock tracking"),
                "text": qsTr("Alt + Click to follow the object"),
                "imageUrl": "image://svg/skin/promo/ptz_promo_tracking.svg"
            })
        }

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
