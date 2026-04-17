// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.mobile.timeline as Timeline

Item
{
    id: placeholder

    property bool active: false
    property int objectsType: Timeline.ObjectsLoader.ObjectsType.motion

    opacity: active ? 1 : 0
    visible: opacity > 0.01

    Behavior on opacity { NumberAnimation { duration: 200 }}

    Column
    {
        id: placeholderColumn

        anchors.verticalCenter: placeholder.verticalCenter
        width: placeholder.width

        padding: 20
        spacing: 24

        ColoredImage
        {
            id: placeholderImage

            anchors.horizontalCenter: placeholderColumn.horizontalCenter
            primaryColor: ColorTheme.colors.mobileTimeline.noContentPlaceholder
            sourceSize: Qt.size(64, 64)

            sourcePath:
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
        }

        Column
        {
            id: textColumn

            spacing: 12

            width: placeholderColumn.width - placeholderColumn.leftPadding
                - placeholderColumn.rightPadding

            Text
            {
                id: placeholderText

                width: textColumn.width
                horizontalAlignment: Text.AlignHCenter
                color: ColorTheme.colors.mobileTimeline.noContentPlaceholder
                font.pixelSize: 16
                font.weight: Font.Medium

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
            }

            Text
            {
                id: placeholderDetailsText

                width: textColumn.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: ColorTheme.colors.mobileTimeline.noContentPlaceholder
                font.pixelSize: 14

                text:
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
        }
    }
}
