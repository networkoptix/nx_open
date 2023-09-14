// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop 1.0

import ".."
import "Components"

Dialog
{
    id: dialog
    objectName: "passwordChangeDialog"

    modality: Qt.ApplicationModal

    minimumWidth: 450
    minimumHeight: passwordGridLayout.y + passwordGridLayout.height + buttonBox.implicitHeight + 16

    width: minimumWidth
    height: minimumHeight

    property string login: ""
    property bool showLogin: false

    property alias currentPassword: currentPasswordTextField.text
    property alias newPassword: newPasswordTextField.text

    property bool askCurrentPassword: true
    property alias text: headerText.text

    property var currentPasswordValidator: null

    title: qsTr("Change password - %1").arg(login)

    color: ColorTheme.colors.dark7

    function openNew()
    {
        dialog.show()
        dialog.raise()
    }

    Item
    {
        id: header
        width: parent.width
        height: horiziontalLine.y + horiziontalLine.height
        visible: headerText.text

        Text
        {
            id: headerText

            font: Qt.font({pixelSize: 16, weight: Font.Medium})
            color: ColorTheme.colors.light10
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 16
        }

        Item
        {
            id: horiziontalLine
            anchors.top: headerText.bottom
            anchors.topMargin: 16
            height: 2
            width: parent.width

            Rectangle
            {
                width: parent.width
                height: 1
                color: ColorTheme.shadow
            }
            Rectangle
            {
                width: parent.width
                height: 1
                y: 1
                color: ColorTheme.dark
            }
        }
    }

    GridLayout
    {
        id: passwordGridLayout

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.visible ? header.bottom : parent.top

        anchors.topMargin: 16
        anchors.leftMargin: 16
        anchors.rightMargin: 16

        columns: 2
        rowSpacing: 8
        columnSpacing: 8

        component Label: Text
        {
            font: Qt.font({pixelSize: 14, weight: Font.Normal})
            color: ColorTheme.colors.light16
            Layout.alignment: Qt.AlignRight | Qt.AlignTop
            Layout.topMargin: 6
        }

        Label
        {
            text: qsTr("Login")
            visible: dialog.showLogin
        }

        TextField
        {
            enabled: false
            Layout.fillWidth: true
            text: dialog.login
            visible: dialog.showLogin
        }

        Label
        {
            text: qsTr("Current password")
            visible: dialog.askCurrentPassword

        }

        PasswordFieldWithWarning
        {
            id: currentPasswordTextField
            visible: dialog.askCurrentPassword

            focus: dialog.askCurrentPassword

            Layout.fillWidth: true
            Layout.bottomMargin: 8
            showStrength: false
            validateFunc: dialog.currentPasswordValidator
        }

        Label
        {
            text: qsTr("New password")
        }

        PasswordFieldWithWarning
        {
            id: newPasswordTextField
            Layout.fillWidth: true

            focus: !dialog.askCurrentPassword

            validateFunc: (text) => UserSettingsGlobal.validatePassword(text)
            onTextChanged: confirmPasswordTextField.warningState = false
        }

        Label
        {
            text: qsTr("Confirm password")
        }

        PasswordFieldWithWarning
        {
            id: confirmPasswordTextField
            Layout.fillWidth: true

            showStrength: false
            validateFunc: (text) =>
            {
                return text == newPasswordTextField.text
                    ? ""
                    : qsTr("Passwords do not match")
            }
        }
    }

    function accept()
    {
        const isTrue = v => !!v

        if ([newPasswordTextField.validate(),
            newPasswordTextField.strengthAccepted,
            confirmPasswordTextField.validate(),
            (!askCurrentPassword || currentPasswordTextField.validate())].every(isTrue))
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
