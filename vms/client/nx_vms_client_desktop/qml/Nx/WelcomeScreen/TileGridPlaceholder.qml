// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx.Core 1.0

Item
{
    /** Describes, what kind of image & text should be visible. */
    enum Mode
    {
        Disabled,
        NoSystemsFound,
        NoFavorites,
        NoHidden
    }

    property int mode: TileGridPlaceholder.Mode.Disabled

    visible: mode !== TileGridPlaceholder.Mode.Disabled

    height: description.y + description.height

    Image
    {
        id: icon

        anchors.horizontalCenter: parent.horizontalCenter

        width: 128
        height: 128

        sourceSize: Qt.size(width, height)
        source:
        {
            switch (parent.mode)
            {
                case TileGridPlaceholder.Mode.NoSystemsFound:
                    return "image://svg/skin/welcome_screen/placeholder/no_systems_found.svg"
                case TileGridPlaceholder.Mode.NoFavorites:
                    return "image://svg/skin/welcome_screen/placeholder/no_favorites.svg"
                case TileGridPlaceholder.Mode.NoHidden:
                    return "image://svg/skin/welcome_screen/placeholder/no_hidden.svg"
                default:
                    return ""
            }
        }
    }

    Text
    {
        id: title

        anchors.horizontalCenter: parent.horizontalCenter

        anchors.top: icon.bottom
        anchors.topMargin: 16

        height: 32

        font.pixelSize: 26

        color: ColorTheme.windowText

        text:
        {
            switch (parent.mode)
            {
                case TileGridPlaceholder.Mode.NoSystemsFound:
                    return qsTr("Nothing Found")
                case TileGridPlaceholder.Mode.NoFavorites:
                    return qsTr("No Favorite Systems")
                case TileGridPlaceholder.Mode.NoHidden:
                    return qsTr("No Hidden Systems")
                default:
                    return ""
            }
        }
    }

    Text
    {
        id: description

        anchors.horizontalCenter: parent.horizontalCenter

        anchors.top: title.bottom
        anchors.topMargin: 8
        width: 380

        color: ColorTheme.windowText

        font.pixelSize: 14

        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap

        text:
        {
            switch (parent.mode)
            {
                case TileGridPlaceholder.Mode.NoFavorites:
                    return qsTr("You can add system to the favorites from the context menu")
                case TileGridPlaceholder.Mode.NoHidden:
                    return qsTr("You can hide systems from the main list from the context menu")
                case TileGridPlaceholder.Mode.NoSystemsFound:
                default:
                    return ""
            }
        }
    }
}
