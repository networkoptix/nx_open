// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

import nx.vms.client.desktop 1.0

DropArea
{
    id: dropArea

    property var currentMimeData: watcher.mimeData

    DragMimeDataWatcher
    {
        id: watcher
        watched: dropArea
    }
}
