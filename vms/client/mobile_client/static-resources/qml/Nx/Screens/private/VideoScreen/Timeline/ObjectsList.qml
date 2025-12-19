// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.mobile.timeline as Timeline

Item
{
    id: view

    property alias chunkProvider: loader.chunkProvider
    property alias objectsType: loader.objectsType
    property alias bottomBoundMs: loader.bottomBoundMs

    property alias startTimeMs: loader.startTimeMs
    property alias durationMs: loader.durationMs
    property alias windowAtLive: loader.topLimited
    readonly property real millisecondsPerPixel: durationMs / Math.max(height, 1)
    readonly property alias bucketSizeMs: loader.bucketSizeMs

    property alias minimumStackDurationMs: loader.minimumStackDurationMs

    property real tileHeight: 100
    property real spacing: 4

    property int fadeDurationMs: 100

    enum Direction { Upward, Downward }
    property int direction: ObjectsList.Upward

    property bool interactive: true

    property alias maxObjectsPerBucket: loader.maxObjectsPerBucket

    readonly property alias objectChunks: loader.objectChunks

    signal tapped(Timeline.MultiObjectData modelData)

    property Component delegate: Component { ObjectsListDelegate {} }
    property Component preloaderDelegate

    function timeToPosition(timeMs)
    {
        const pos = (timeMs - startTimeMs) / millisecondsPerPixel
        return direction === ObjectsList.Upward ? (height - pos) : pos
    }

    clip: true

    Timeline.ObjectsLoader
    {
        id: loader

        bucketSizeMs: Math.round(view.millisecondsPerPixel * (view.tileHeight + view.spacing))

        onBucketsReset:
        {
            for (let index = 0; index < d.holders.length; ++index)
                d.releaseItem(index, /*animated*/ true)

            d.holders.length = loader.count()

            for (let index = 0; index < d.holders.length; ++index)
                d.ensureItem(index, loader.bucket(index))
        }

        onScrolledIn: (first, count) =>
        {
            const end = first + count
            for (let index = first; index < end; ++index)
            {
                d.holders.splice(index, 0, undefined)
                d.ensureItem(index, loader.bucket(index))
            }
        }

        onScrolledOut: (first, count) =>
        {
            for (let i = 0; i < count; ++i)
                d.releaseItem(first + i, /*animated*/ false)

            d.holders.splice(first, count)
        }

        onBucketChanged: (index) =>
        {
            d.holders[index].bucket = loader.bucket(index)
        }
    }

    Component
    {
        id: delegateHolderComponent

        Item
        {
            id: delegateHolder

            property var bucket
            property Item item
            property bool pooled: false
            property bool fadingOut: false

            readonly property bool isPreloaderNeeded: !pooled && !fadingOut
                && !loader.synchronous
                && bucket?.state === Timeline.ObjectBucket.Initial
                && bucket.isLoading

            signal fadeOutFinished()

            y:
            {
                const pos = view.timeToPosition(bucket?.startTimeMs ?? 0)
                return view.direction === ObjectsList.Upward ? (pos - height) : pos
            }

            width: view.width
            height: view.tileHeight + view.spacing
            visible: !pooled && !!bucket

            Loader
            {
                id: delegateLoader

                readonly property var modelData: delegateHolder.bucket?.data
                readonly property ObjectsList objectsList: view

                sourceComponent: view.delegate

                y: view.spacing / 2
                width: delegateHolder.width
                height: view.tileHeight

                // Default state is just created or pooled.
                opacity: 0.0

                visible: opacity > 0.0
                layer.enabled: opacity < 1.0

                states: [
                    State
                    {
                        name: "InitialOrEmpty"
                        when: !delegateHolder.pooled && !delegateHolder.fadingOut
                            && (delegateHolder.bucket?.state === Timeline.ObjectBucket.Initial
                                || delegateHolder.bucket?.state === Timeline.ObjectBucket.Empty)
                    },

                    State
                    {
                        name: "Ready"
                        when: !delegateHolder.pooled && !delegateHolder.fadingOut
                            && (delegateHolder.bucket?.state === Timeline.ObjectBucket.Ready)
                        PropertyChanges { target: delegateLoader; opacity: 1.0 }
                    },

                    State
                    {
                        name: "FadingOut"
                        when: delegateHolder.fadingOut
                    }
                ]

                transitions: [
                    Transition
                    {
                        from: "InitialOrEmpty"
                        to: "Ready"
                        reversible: true
                        NumberAnimation { property: "opacity"; duration: view.fadeDurationMs }
                    },

                    Transition
                    {
                        from: "Ready"
                        to: "FadingOut"

                        SequentialAnimation
                        {
                            NumberAnimation { property: "opacity"; duration: view.fadeDurationMs }
                            ScriptAction { script: delegateHolder.fadeOutFinished() }
                        }
                    },

                    Transition
                    {
                        from: "*"
                        to: "FadingOut"
                        ScriptAction { script: Qt.callLater(() => delegateHolder.fadeOutFinished()) }
                    }
                ]
            }

            Loader
            {
                readonly property var startTimeMs: delegateHolder.bucket?.startTimeMs ?? 0
                readonly property real durationMs: view.bucketSizeMs

                sourceComponent: view.preloaderDelegate
                anchors.fill: delegateHolder

                opacity: delegateHolder.isPreloaderNeeded ? 1.0 : 0.0
                visible: opacity > 0.0
            }

            TapHandler
            {
                id: tapHandler

                enabled: view.interactive
                    && delegateHolder.bucket?.state === Timeline.ObjectBucket.Ready

                // Ensure the handler takes exclusive grab on press.
                gesturePolicy: TapHandler.WithinBounds

                onTapped:
                    view.tapped(delegateHolder.bucket.data)
            }
        }
    }

    NxObject
    {
        id: d

        property var holders: []
        property var pooledHolders: []

        function ensureItem(index, bucket)
        {
            let holder = holders[index]
            if (holder)
            {
                holder.bucket = bucket
                return
            }

            holder = pooledHolders.pop()
            if (holder)
            {
                holder.bucket = bucket
                holder.fadingOut = false //< Just to be safe.
                holder.pooled = false
            }
            else
            {
                holder = delegateHolderComponent.createObject(view, {bucket: bucket})
                holder.fadeOutFinished.connect(() => poolHolder(holder))
            }

            holders[index] = holder
        }

        function releaseItem(index, animated)
        {
            const holder = holders[index]
            if (!holder)
                return

            holders[index] = undefined
            if (animated)
                holder.fadingOut = true
            else
                poolHolder(holder)
        }

        function poolHolder(holder)
        {
            if (!holder)
                return

            holder.bucket = undefined
            holder.fadingOut = false
            holder.pooled = true
            pooledHolders.push(holder)
        }
    }
}
