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
