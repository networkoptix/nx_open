// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Qt5Compat.GraphicalEffects
import QtQuick

import Nx.Core

Item
{
    id: remoteImage

    property string requestLine

    property string imagePath: requestLine
        ? `image://remote/${remoteImage.requestLine}`
        : ""

    // An optional image to display while the main image is being loaded.
    property string fallbackImagePath

    // Whether to show the fallback image (if present) in cases when `errorText` would be shown.
    property bool fallbackImageIfError: false

    property color backgroundColor: ColorTheme.colors.dark7
    property color foregroundColor: ColorTheme.colors.light16
    property color frameColor: ColorTheme.colors.dark10

    property real roundingRadius: 4
    property string errorText: qsTr("NO DATA")

    property alias font: noData.font
    property alias preloader: preloader

    Rectangle
    {
        id: background

        anchors.fill: parent
        radius: remoteImage.roundingRadius
        color: remoteImage.backgroundColor
        border.color: remoteImage.frameColor
    }

    Item
    {
        id: imageHolder

        readonly property bool hasImage: image.status === Image.Ready
            || (fallbackImage.status === Image.Ready && fallbackImage.isRelevant)

        anchors.fill: remoteImage
        visible: false //< maskedImage is visible instead.

        Image
        {
            id: fallbackImage

            readonly property bool isRelevant: image.status === Image.Loading
                || (image.isError && remoteImage.fallbackImageIfError)

            anchors.fill: imageHolder
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
            fillMode: Image.PreserveAspectFit
            source: remoteImage.fallbackImagePath
            visible: isRelevant
        }

        Image
        {
            id: image

            readonly property bool isError: !!remoteImage.imagePath
                && image.status !== Image.Loading
                && image.paintedWidth === 0

            anchors.fill: imageHolder
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            retainWhileLoading: true
            source: remoteImage.imagePath
            cache: false //< RemoteAsyncImageProvider does all necessary caching.
        }
    }

    // A mask to round corners of the image.
    OpacityMask
    {
        id: maskedImage

        anchors.fill: remoteImage
        source: imageHolder
        maskSource: background

        opacity: imageHolder.hasImage ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 100 }}
    }

    Text
    {
        id: noData

        anchors.fill: remoteImage
        verticalAlignment: Qt.AlignVCenter
        horizontalAlignment: Qt.AlignHCenter
        color: remoteImage.foregroundColor
        text: remoteImage.errorText

        opacity: (image.isError && !imageHolder.hasImage) ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 100 }}
    }

    NxDotPreloader
    {
        id: preloader

        anchors.centerIn: remoteImage
        color: remoteImage.foregroundColor
        running: image.status === Image.Loading && !!requestLine
    }
}
