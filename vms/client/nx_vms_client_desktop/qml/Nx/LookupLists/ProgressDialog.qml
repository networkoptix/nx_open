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

    required property string processName

    // Properties isVisibleCancelButton and isVisibleDoneButton
    // shouldn't be changed outside this file and must be treated by external user as readonly.
    property bool isVisibleCancelButton: false
    property bool isVisibleDoneButton: false

    minimumWidth: 450
    minimumHeight: 135
    maximumHeight: minimumHeight
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    modality: Qt.ApplicationModal

    signal progressFinished
    signal progressStarted
    signal doneClicked

    buttonBox: DialogButtonBox
    {
        alignment: Qt.AlignUndefined
        contentItem: Item
        {
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
                onClicked:
                {
                    control.doneClicked()
                    control.accept()
                }
            }
        }
    }

    onProgressStarted:
    {
        control.visible = true
        progressBar.title = qsTr(processName)
        progressBar.indeterminate = true
        isVisibleCancelButton = true
        isVisibleDoneButton = false
    }

    onProgressFinished:
    {
        progressBar.title = qsTr("Finished")
        progressBar.indeterminate = false
        isVisibleCancelButton = false
        isVisibleDoneButton = true
    }

    ProgressBar
    {
        id: progressBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        value: 1.0
    }
}
