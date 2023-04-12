// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Controls
import Nx.Dialogs

ModalDialog
{
    id: dialog

    property bool editMode: false
    property alias listName: nameField.text
    property alias columnName: columnNameField.text

    signal deleteRequested()

    title: editMode ? qsTr("List Settings") : qsTr("New List")

    contentItem: GridLayout
    {
        columns: 2

        component AlignedLabel: Label
        {
            Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
        }

        AlignedLabel { text: qsTr("Name") }

        TextField
        {
            id: nameField
            Layout.fillWidth: true
        }

        AlignedLabel { text: qsTr("Column Name") }

        TextField
        {
            id: columnNameField
            Layout.fillWidth: true
        }
    }

    function accept()
    {
        if (nameField.text && columnNameField.text)
        {
            dialog.accepted()
            dialog.close()
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Cancel

        Button
        {
            text: editMode ? qsTr("OK") : qsTr("Create")
            width: Math.max(buttonBox.standardButton(DialogButtonBox.Cancel).width, implicitWidth)
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: nameField.text && columnNameField.text
            isAccentButton: true
        }
    }

    TextButton
    {
        x: 8
        anchors.verticalCenter: buttonBox.verticalCenter
        visible: editMode
        text: qsTr("Delete")
        DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        icon.source: "image://svg/skin/text_buttons/trash.svg"
        icon.width: 20
        icon.height: 20
        onClicked:
        {
            dialog.deleteRequested()
            dialog.reject()
        }
    }
}
