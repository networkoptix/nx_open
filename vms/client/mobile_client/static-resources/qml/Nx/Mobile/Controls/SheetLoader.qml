// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

/**
 * Loader for Sheet-like components instantiated inside non-visual items.
 */
NxObject
{
    id: loader

    default required property Component delegate

    readonly property alias sheet: d.sheet
    readonly property bool opened: sheet?.opened ?? false

    function open()
    {
        if (!d.sheet)
            d.sheet = delegate.createObject(loader)

        d.sheet.open()
    }

    NxObject
    {
        id: d

        property var sheet: null
    }
}
