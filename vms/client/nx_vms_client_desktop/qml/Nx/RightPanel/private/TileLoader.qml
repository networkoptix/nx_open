// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

Loader
{
    id: loader

    property TileController tileController: null

    source:
    {
        if (!model || !model.display)
            return "tiles/SeparatorTile.qml"

        if (model.progressValue !== undefined)
            return "tiles/ProgressTile.qml"

        return brief
            ? "tiles/BriefTile.qml"
            : "tiles/InfoTile.qml"
    }

    onVisibleChanged: model.visible = visible
    Component.onCompleted: model.visible = visible
    Component.onDestruction: model.visible = false

    onLoaded:
        item.controller = loader.tileController
}
