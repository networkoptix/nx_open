// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Items

import nx.vms.client.mobile.timeline as Timeline

Placeholder
{
    id: placeholder

    property bool active: false
    property int objectsType: Timeline.ObjectsLoader.ObjectsType.motion

    property int fadeDurationMs: 200

    opacity: active ? 1 : 0
    visible: opacity > 0.01

    Behavior on opacity { NumberAnimation { duration: placeholder.fadeDurationMs }}

    imageColor: ColorTheme.colors.mobileTimeline.contentPlaceholder.icon
    textColor: ColorTheme.colors.mobileTimeline.contentPlaceholder.caption
    descriptionColor: ColorTheme.colors.mobileTimeline.contentPlaceholder.description

    imageSource:
    {
        switch (placeholder.objectsType)
        {
            case Timeline.ObjectsLoader.ObjectsType.bookmarks:
                return "image://skin/64x64/Outline/nobookmarks.svg"

            case Timeline.ObjectsLoader.ObjectsType.analytics:
                return "image://skin/64x64/Outline/noobjects.svg"

            case Timeline.ObjectsLoader.ObjectsType.motion:
                return "image://skin/64x64/Outline/motion.svg"
        }
    }

    text:
    {
        switch (placeholder.objectsType)
        {
            case Timeline.ObjectsLoader.ObjectsType.bookmarks:
                return qsTr("No Bookmarks")

            case Timeline.ObjectsLoader.ObjectsType.analytics:
                return qsTr("No Objects")

            case Timeline.ObjectsLoader.ObjectsType.motion:
                return qsTr("No Motion")
        }
    }

    description:
    {
        switch (placeholder.objectsType)
        {
            case Timeline.ObjectsLoader.ObjectsType.bookmarks:
                return qsTr("No bookmarks have been created on this timeline")

            case Timeline.ObjectsLoader.ObjectsType.analytics:
                return qsTr("No objects have been detected on this timeline")

            case Timeline.ObjectsLoader.ObjectsType.motion:
                return qsTr("No motion has been detected on this timeline")
        }
    }
}
