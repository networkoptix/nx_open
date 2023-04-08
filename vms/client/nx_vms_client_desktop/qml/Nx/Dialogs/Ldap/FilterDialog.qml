// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop 1.0

import "../UserManagement/Components"

Dialog
{
    id: dialog

    modality: Qt.ApplicationModal

    minimumWidth: 500
    minimumHeight: content.height + buttonBox.implicitHeight + 32
    maximumHeight: minimumHeight

    width: minimumWidth
    height: minimumHeight

    property alias name: nameTextField.text
    property alias baseDn: baseDnTextField.text
    property alias filter: filterTextField.text

    property var self
    property var testState: LdapSettings.TestState.initial

    property string testMessage: ""

    title: qsTr("Edit Filter")

    color: ColorTheme.colors.dark7

    function openNew()
    {
        dialog.show()
        dialog.raise()
    }

    Column
    {
        id: content

        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16

        spacing: 8

        CenteredField
        {
            text: qsTr("Name")

            leftSideMargin: 78
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: nameTextField

                width: parent.width
                focus: true
            }
        }

        CenteredField
        {
            text: qsTr("Base DN")

            leftSideMargin: 78
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: baseDnTextField
                width: parent.width
                validateFunc: (text) =>
                {
                    return text == ""
                        ? qsTr("Base DN cannot be empty")
                        : ""
                }
            }
        }

        CenteredField
        {
            text: qsTr("Filter")

            leftSideMargin: 78
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: filterTextField
                width: parent.width
            }
        }
    }

    function accept()
    {
        if (baseDnTextField.validate())
        {
            dialog.accepted()
            dialog.close()
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout

        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }
}
