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
    property alias model: repeater.model

    title: qsTr("Add Entry")

    contentItem: Column
    {
        topPadding: padding / 2
        spacing: 6

        Repeater
        {
            id: repeater

            RowLayout
            {
                spacing: 6
                width: parent.width

                Label
                {
                    Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
                    text: display
                }
                TextField
                {
                    Layout.fillWidth: true
                    text: edit
                    onTextChanged: model.edit = text
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Cancel

        Button
        {
            text: qsTr("Add")
            width: Math.max(buttonBox.standardButton(DialogButtonBox.Cancel).width, implicitWidth)
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            //enabled: nameField.text && columnNameField.text
            isAccentButton: true
        }
    }
}
