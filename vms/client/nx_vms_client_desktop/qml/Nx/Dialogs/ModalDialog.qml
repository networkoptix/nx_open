// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Dialogs

Dialog
{
    id: dialog
    modality: Qt.ApplicationModal

    minimumWidth: 450
    minimumHeight: contentItem.implicitHeight + (buttonBox ? buttonBox.height : 0) + padding * 2
    maximumWidth: minimumWidth
    maximumHeight: minimumHeight

    width: minimumWidth
    height: minimumHeight

    property real padding: 16

    function openNew(x, y)
    {
        dialog.x = x - dialog.width / 2
        dialog.y = y - dialog.height / 2
        dialog.show()
        dialog.raise()
    }

    function openNewIn(control)
    {
        const x = control.x + (control.width / 2)
        const y = control.y + (control.height / 2)
        return openNew(x, y)
    }
}
