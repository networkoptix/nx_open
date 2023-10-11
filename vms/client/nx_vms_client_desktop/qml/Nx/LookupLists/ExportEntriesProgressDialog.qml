// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Controls
import Nx.Dialogs

ProgressDialog
{
    id: control
    title: qsTr("Export Lists")
    processName: qsTr("Exporting")

    signal openFolderRequested

    buttonBox: DialogButtonBox
    {
        alignment: Qt.AlignUndefined
        contentItem: Item
        {
            TextButton
            {
                anchors.left: parent.left
                anchors.margins: 16
                text: qsTr("Open File Folder...")
                icon.source: "image://svg/skin/text_buttons/folder_20x20.svg"
                onClicked: control.openFolderRequested()
            }

            Button
            {
                anchors.right: parent.right
                anchors.margins: 16
                visible: isVisibleCancelButton
                text: qsTr("Cancel")
                onClicked: control.reject()
            }

            Button
            {
                anchors.right: parent.right
                anchors.margins: 16
                visible: isVisibleDoneButton
                text: qsTr("Done")
                isAccentButton: true
                onClicked: control.accept()
            }
        }
    }
}
