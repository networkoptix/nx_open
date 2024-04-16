// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx

Item
{
    id: item

    property string sourcePath //< Relative to core::Skin paths.

    // Predefined color channel overrides. Non-SVG images support only `primaryColor`.
    property var primaryColor: undefined
    property var secondaryColor: undefined
    property var tertiaryColor: undefined

    // Additional color customization, as a map `name`-->`color or colorName`.
    property var customColors: ({})

    property alias sourceSize: image.sourceSize
    property alias fillMode: image.fillMode
    property alias mirror: image.mirror
    property alias mirrorVertically: image.mirrorVertically
    property alias horizontalAlignment: image.horizontalAlignment
    property alias verticalAlignment: image.verticalAlignment

    readonly property alias status: image.status
    readonly property alias paintedWidth: image.paintedWidth
    readonly property alias paintedHeight: image.paintedHeight

    property string name //< For autotesting.

    implicitWidth: image.implicitWidth
    implicitHeight: image.implicitHeight

    Image
    {
        id: image

        anchors.fill: item

        source:
        {
            if (!item.sourcePath)
                return ""

            const url = NxGlobals.url(item.sourcePath)
            let customization = url.queryMap()

            for (let customChannel in item.customColors)
                customization[customChannel] = item.customColors[customChannel]

            if (item.primaryColor !== undefined)
                customization["primary"] = item.primaryColor

            if (item.secondaryColor !== undefined)
                customization["secondary"] = item.secondaryColor

            if (item.tertiaryColor !== undefined)
                customization["tertiary"] = item.tertiaryColor

            let query = []

            for (let channel in customization)
            {
                query.push(encodeURIComponent(channel) + "="
                    + encodeURIComponent(customization[channel]))
            }

            let path = url.path()
            if (path[0] != "/")
                path = "/" + path

            const schemeAndPath = "image://skin" + path

            return query.length
                ? `${schemeAndPath}?${query.join("&")}`
                : schemeAndPath
        }
    }
}
