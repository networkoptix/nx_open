// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Layouts 1.0
import Qt.labs.platform 1.0

import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop.debug 1.0

Dialog
{
    title: "QmlDialogWrapper Test"
    width: 400
    height: 120

    contentItem: Item
    {
        RowLayout
        {
            width: parent.width
            spacing: 8

            Label
            {
                text: "QML File:"
            }

            TextField
            {
                id: filePath
                Layout.fillWidth: true
                text: "qrc:/qml/Nx/Dialogs/Test/DialogWrapperTestDialog.qml"
            }

            Button
            {
                text: "\u2026" //< Ellipsis.
                onClicked: fileDialog.open()
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        standardButtons: DialogButtonBox.Close

        Button
        {
            text: "Test"
            width: Math.max(implicitWidth, 80)
            isAccentButton: true

            onClicked:
            {
                dialogWrapper.source = filePath.text
                dialogWrapper.open()
            }
        }
    }

    FileDialog
    {
        id: fileDialog
        title: "Choose QML File"
        nameFilters: ["QML files (*.qml)"]

        onAccepted: filePath.text = file
    }

    QmlDialogWrapper
    {
        id: dialogWrapper
    }
}
