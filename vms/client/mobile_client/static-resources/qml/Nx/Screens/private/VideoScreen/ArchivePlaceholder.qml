// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Items

Rectangle
{
    id: placeholder

    property bool loading: true
    property bool hasArchive: true
    property bool canViewArchive: true

    property int fadeDurationMs: 200

    color: "transparent"
    clip: true

    opacity: 0
    visible: opacity > 0

    implicitWidth: Math.max(mainPlaceholder.implicitWidth, preloaderColumn.width)
    implicitHeight: Math.max(mainPlaceholder.implicitHeight, preloaderColumn.height)

    Placeholder
    {
        id: mainPlaceholder

        imageColor: ColorTheme.colors.mobileTimeline.archivePlaceholder.icon
        textColor: ColorTheme.colors.mobileTimeline.archivePlaceholder.caption
        descriptionColor: ColorTheme.colors.mobileTimeline.archivePlaceholder.description

        anchors.fill: placeholder
        clip: false

        opacity: 0
        visible: opacity > 0

        imageSource:
        {
            if (!placeholder.canViewArchive)
                return "image://skin/64x64/Outline/no_access.svg"

            if (!placeholder.hasArchive)
                return "image://skin/64x64/Outline/no_information.svg"

            return ""
        }

        text:
        {
            if (!placeholder.canViewArchive)
                return qsTr("Not Available")

            if (!placeholder.hasArchive)
                return qsTr("No Archive")

            return ""
        }

        description:
        {
            if (!placeholder.canViewArchive)
                return qsTr("You do not have permission to view the archive")

            if (!placeholder.hasArchive)
                return qsTr("You do not have any recorded video in the archive")

            return ""
        }
    }

    Column
    {
        id: preloaderColumn

        anchors.centerIn: placeholder

        leftPadding: mainPlaceholder.horizontalPadding
        rightPadding: mainPlaceholder.horizontalPadding
        spacing: 16

        opacity: 0
        visible: opacity > 0

        NxDotPreloader
        {
            id: preloaderIndicator

            anchors.horizontalCenter: preloaderColumn.horizontalCenter
            color: mainPlaceholder.descriptionColor
            dotRadius: 6
            spacing: 8
        }

        Text
        {
            id: preloaderText

            anchors.horizontalCenter: preloaderColumn.horizontalCenter
            horizontalAlignment: Text.AlignHCenter

            font.pixelSize: 16
            color: mainPlaceholder.descriptionColor
            text: qsTr("Timeline is loading...")
        }
    }

    states:
    [
        State
        {
            name: "MainPlaceholder"
            when: !placeholder.canViewArchive || (!placeholder.hasArchive && !placeholder.loading)

            PropertyChanges
            {
                placeholder { opacity: 1.0 }
                mainPlaceholder { opacity: 1.0 }
            }
        },

        State
        {
            name: "LoadingPlaceholder"
            when: placeholder.canViewArchive && !placeholder.hasArchive && placeholder.loading

            PropertyChanges
            {
                placeholder { opacity: 1.0 }
                preloaderColumn { opacity: 1.0 }
            }
        }
    ]

    component OpacityAnimation: NumberAnimation
    {
        property: "opacity"
        duration: placeholder.fadeDurationMs
    }

    transitions: [
        Transition
        {
            // Loading placeholder -> main placeholder:
            // - main control becomes opaque immediately, no transition animation needed
            // - fade out the loading placeholder
            // - then fade in the main placeholder

            from: "LoadingPlaceholder"
            to: "MainPlaceholder"

            SequentialAnimation
            {
                OpacityAnimation { target: preloaderColumn  }
                OpacityAnimation { target: mainPlaceholder }
            }
        },

        Transition
        {
            // No placeholder -> main placeholder:
            // - main control becomes opaque immediately, no transition animation needed
            // - fade in the main placeholder

            from: ""
            to: "MainPlaceholder"
            OpacityAnimation { target: mainPlaceholder }
        },

        Transition
        {
            // Any placeholder -> no placeholder:
            // - fade out the whole control
            // - then instantly return the loading and the main placeholders' opacity to zero

            to: ""

            SequentialAnimation
            {
                OpacityAnimation { target: placeholder }
                PropertyAction { target: preloaderColumn; property: "opacity" }
                PropertyAction { target: mainPlaceholder; property: "opacity" }
            }
        }
    ]
}
