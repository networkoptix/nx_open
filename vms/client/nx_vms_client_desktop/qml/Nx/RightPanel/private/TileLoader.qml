// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import "tiles"

Loader
{
    id: loader

    property TileController tileController: null

    Component
    {
        id: separatorTile
        SeparatorTile { }
    }

    Component
    {
        id: progressTile
        ProgressTile { }
    }

    Component
    {
        id: briefTile
        BriefTile { }
    }

    Component
    {
        id: infoTile
        InfoTile { }
    }

    sourceComponent:
    {
        if (!model || !model.display)
            return separatorTile

        if (model.progressValue !== undefined)
            return progressTile

        return brief
            ? briefTile
            : infoTile
    }

    onVisibleChanged: model.visible = visible
    Component.onCompleted: model.visible = visible
    Component.onDestruction: model.visible = false

    onLoaded:
        item.controller = loader.tileController
}
