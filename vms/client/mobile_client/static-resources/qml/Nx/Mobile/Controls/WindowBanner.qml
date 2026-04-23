// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

Control
{
    id: control

    property BannerSource currentItem: null
    readonly property var activeItems: Array.from(children).filter(
        item => (item instanceof BannerSource) && item.active)

    visible: !!currentItem

    function closeCurrent()
    {
        if (currentItem && currentItem.closeable)
            currentItem.active = false
    }

    function showActive()
    {
        currentItem = activeItems[0] ?? null
    }

    onCurrentItemChanged: timer.reset()

    onActiveItemsChanged:
    {
        if (!currentItem)
            showActive()
    }

    contentItem: Banner
    {
        id: banner

        shown: currentItem?.active ?? false

        text: currentItem?.text ?? ""
        type: currentItem?.type ?? Banner.Info
        action: currentItem?.action ?? null
        closeable: currentItem?.closeable ?? true

        onCloseClicked: closeCurrent()
        onClosed: showActive()
    }

    Timer
    {
        id: timer

        function reset()
        {
            interval = currentItem?.durationMs ?? 0

            if (interval > 0)
                timer.restart()
            else
                timer.stop()
        }

        onTriggered: closeCurrent()
    }

    function show(text, type)
    {
        defaultBanners.model.append({text: text, type: type})
    }

    Repeater
    {
        id: defaultBanners

        model: ListModel { }

        delegate: BannerSource
        {
            id: source

            readonly property bool actual: active || control.currentItem === source

            active: true
            text: model?.text ?? ""
            type: model?.type ?? Banner.Info
            closeable: true

            onActualChanged:
            {
                if (!actual)
                    defaultBanners.model.remove(index)
            }
        }
    }
}
