// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import "tiles"

Loader
{
    id: loader

    property TileController tileController: null

    source:
    {
        if (!model || !model.display)
            return "private/tiles/SeparatorTile.qml"

        if (model.progressValue !== undefined)
            return "private/tiles/ProgressTile.qml"

        return brief
            ? "private/tiles/BriefTile.qml"
            : "private/tiles/InfoTile.qml"
    }

    onVisibleChanged:
        model.visible = visible

    Component.onCompleted:
        model.visible = visible

    Component.onDestruction:
        model.visible = false

    Binding
    {
        target: item
        when: item && item.hasOwnProperty("controller")
        property: "controller"
        value: loader.tileController
    }
}
