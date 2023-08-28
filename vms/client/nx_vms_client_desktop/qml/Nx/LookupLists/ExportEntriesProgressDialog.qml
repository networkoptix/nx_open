// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Controls
import Nx.Dialogs

Dialog
{
    id: control

    title: qsTr("Export Lists")
    minimumWidth: 450
    minimumHeight: 135
    maximumHeight: minimumHeight
    modality: Qt.ApplicationModal
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    signal progressFinished
    signal progressStarted
    signal openFolderRequested

    ProgressBar
    {
        id: progressBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        value: 1.0
    }


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
                id: cancelButton

                anchors.right: parent.right
                anchors.margins: 16
                text: qsTr("Cancel")
                onClicked: control.reject()
            }

            Button
            {
                id: doneButton

                anchors.right: parent.right
                anchors.margins: 16
                visible: false
                text: qsTr("Done")
                isAccentButton: true
                onClicked: control.accept()
            }
        }
    }

    onProgressStarted:
    {
        control.visible = true
        progressBar.title = qsTr("Exporting")
        progressBar.indeterminate = true
        cancelButton.visible = true
        doneButton.visible = false
    }

    onProgressFinished:
    {
        progressBar.title = qsTr("Finished")
        progressBar.indeterminate = false
        cancelButton.visible = false
        doneButton.visible = true
    }
}
