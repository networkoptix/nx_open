// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

import "components/private"

Item
{
    property alias header: placeholder.header
    property alias description: placeholder.description
    property bool loading: false

    Placeholder
    {
        id: placeholder

        anchors.fill: parent

        visible: !loading
        imageSource: "image://svg/skin/analytics_icons/placeholders/no_settings.svg"
    }

    NxDotPreloader
    {
        id: preloader

        anchors.centerIn: parent

        running: loading
        color: ColorTheme.colors.dark17
    }
}
