// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQml 2.15

import Nx.Core 1.0

Item
{
    id: sceneBanners

    property real maximumWidth: 1024 //< A sensible default.
    property int maximumCount: 7

    property int defaultTimeoutMs: 3000
    property int showDurationMs: 200
    property int hideDurationMs: 200

    function add(text, timeoutMs, font) // -> uuid
    {
        return d.add(text, timeoutMs, font)
    }

    function remove(uuid, immediately)
    {
        return d.remove(uuid, immediately)
    }

    function changeText(uuid, text)
    {
        return d.changeText(uuid, text)
    }

    signal removed(var uuid) // -> bool

    width: Math.max(listView.width, 1) //< May not be zero or ListView won't populate.

    height:
    {
        const granularity = 64 //< To avoid frequent resizes a fixed size granularity is used.
        return Math.round(listView.height / granularity + 1.0) * granularity
    }

    ListView
    {
        id: listView

        width: Math.min(contentItem.childrenRect.width, sceneBanners.maximumWidth)
        height: contentItem.childrenRect.height
        anchors.centerIn: sceneBanners

        interactive: false
        spacing: 2

        model: ListModel
        {
            id: listModel
        }

        delegate: Component
        {
            Item
            {
                id: bannerHolder

                implicitWidth: banner.implicitWidth
                implicitHeight: banner.implicitHeight

                width: Math.min(implicitWidth, sceneBanners.maximumWidth)

                layer.enabled: opacity < 1

                readonly property bool immediateRemove: model.immediateRemove

                SceneBanner
                {
                    id: banner

                    text: model.text
                    anchors.horizontalCenter: bannerHolder.horizontalCenter
                    width: Math.min(implicitWidth, bannerHolder.width)

                    Binding
                    {
                        target: banner
                        property: "font"
                        when: !!d.fontOverrides[model.uuid]
                        value: d.fontOverrides[model.uuid]
                        restoreMode: Binding.RestoreNone
                    }
                }

                ListView.onAdd:
                {
                    heightAnimationBinding.when = listView.count > 1
                    addAnimation.start()
                }

                ListView.onRemove:
                {
                    if (bannerHolder.immediateRemove)
                        return

                    heightAnimationBinding.when = listView.count > 1
                    removeAnimation.start()
                }

                NumberAnimation
                {
                    id: addAnimation

                    target: bannerHolder
                    property: "opacity"
                    from: 0
                    to: 1
                    easing.type: Easing.InQuad
                    duration: sceneBanners.showDurationMs

                    onStopped:
                        heightAnimationBinding.when = false
                }

                SequentialAnimation
                {
                    id: removeAnimation

                    PropertyAction
                    {
                        target: bannerHolder
                        property: "ListView.delayRemove"
                        value: true
                    }

                    NumberAnimation
                    {
                        target: bannerHolder
                        property: "opacity"
                        from: 1
                        to: 0
                        easing.type: Easing.OutQuad
                        duration: sceneBanners.hideDurationMs
                    }

                    PropertyAction
                    {
                        target: bannerHolder
                        property: "ListView.delayRemove"
                        value: false
                    }
                }

                Binding
                {
                    id: heightAnimationBinding
                    // Uses opacity as height multiplier during certain add/remove animations.

                    target: banner
                    property: "height"
                    when: false
                    value: bannerHolder.opacity * banner.implicitHeight
                    restoreMode: Binding.RestoreBindingOrValue
                }

                Binding
                {
                    target: banner
                    property: "y"
                    when: removeAnimation.running
                    value: bannerHolder.height - banner.height
                    restoreMode: Binding.RestoreValue
                }
            }
        }
    }

    QtObject
    {
        id: d

        property var fontOverrides: ({}) //< uuidStr -> font

        function add(text, timeoutMs, font)
        {
            const uuid = NxGlobals.generateUuid()

            if (timeoutMs === undefined)
                timeoutMs = sceneBanners.defaultTimeoutMs

            const timer = timeoutMs > 0
                ? Qt.createQmlObject("import QtQuick 2.0; Timer {}", d)
                : null

            if (timer)
            {
                timer.interval = timeoutMs
                timer.repeat = false
                timer.running = true
                timer.triggered.connect(function() { d.remove(uuid); timer.destroy() })
            }

            // Store uuid as string because of ListModel limitations.
            const uuidStr = uuid.toString()

            if (font)
                d.fontOverrides[uuidStr] = font

            listModel.append({"uuid": uuidStr, "text": text, "immediateRemove": false})
            truncateToMaximumCount()

            return uuid
        }

        function find(uuidStr) // -> int
        {
            for (let i = 0; i < listModel.count; ++i)
            {
                if (listModel.get(i).uuid === uuidStr)
                    return i
            }

            return -1
        }

        function remove(uuid, immediately)
        {
            const uuidStr = uuid.toString()
            const index = d.find(uuidStr)
            if (index < 0)
                return false

            if (immediately)
                listModel.setProperty(index, "immediateRemove", true)

            listModel.remove(index)

            if (d.fontOverrides[uuidStr])
                delete d.fontOverrides[uuidStr]

            sceneBanners.removed(uuid)
            return true
        }

        function changeText(uuid, text)
        {
            const index = d.find(uuid.toString())
            if (index < 0)
                return false

            listModel.setProperty(index, "text", text)
            return true
        }

        function truncateToMaximumCount()
        {
            while (listModel.count > sceneBanners.maximumCount)
                remove(listModel.get(0).uuid, /*immediately*/ true)
        }
    }

    onMaximumCountChanged:
        d.truncateToMaximumCount()
}
