// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Dialogs 1.0 as Dialogs

DialogBase
{
    id: dialog

    property alias title: dialogTitle.text
    property alias message: dialogMessage.text
    property alias buttonsModel: buttonBox.buttonsModel

    signal buttonClicked(string buttonId)

    Column
    {
        width: parent.width

        DialogTitle
        {
            id: dialogTitle
            visible: text
        }

        DialogMessage
        {
            id: dialogMessage
            visible: text
            topPadding: dialogTitle.visible ? 0 : 16
        }

        Dialogs.DialogButtonBox
        {
            id: buttonBox
            visible: buttonsModel.length > 0
            onButtonClicked:
            {
                close()
                dialog.buttonClicked(buttonId)
            }
        }
    }
}
