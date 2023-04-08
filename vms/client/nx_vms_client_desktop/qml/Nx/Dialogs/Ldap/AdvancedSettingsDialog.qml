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

    property alias allowDigest: digestCheckBox.checked

    property alias loginAttribute: loginAttributeTextField.text
    property alias groupObjectClass: groupAttributeTextField.text
    property alias memberAttribute: userMembershipAttributeTextField.text

    property alias syncTimeoutS: syncTimeoutSpinBox.value

    modality: Qt.ApplicationModal

    minimumWidth: 540
    minimumHeight: content.height + buttonBox.implicitHeight + 16

    width: minimumWidth
    height: minimumHeight

    title: qsTr("LDAP - Advanced Settings")

    color: ColorTheme.colors.dark7

    function openNew()
    {
        dialog.show()
        dialog.raise()
    }

    Column
    {
        id: content

        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16

        spacing: 8

        SectionHeader
        {
            text: qsTr("Users")
        }

        CenteredField
        {
            text: qsTr("Login Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: loginAttributeTextField

                width: parent.width
                focus: true
                textField.placeholderText: "cn"
            }
        }

        CenteredField
        {
            text: qsTr("Full Name Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: fullNameAttributeTextField

                width: parent.width
                textField.placeholderText: "displayName"
            }
        }

        CenteredField
        {
            text: qsTr("Email Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: emailAttributeTextField

                width: parent.width
                textField.placeholderText: "mail"
            }
        }

        CenteredField
        {
            leftSideMargin: 180
            rightSideMargin: 0

            CheckBox
            {
                id: digestCheckBox
                x: -3
                y: 6
                width: parent.width - x
                text: qsTr("Allow digest authentication for imported users")
                wrapMode: Text.WordWrap
            }
        }

        SectionHeader
        {
            text: qsTr("Groups")
        }

        CenteredField
        {
            text: qsTr("Name Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: groupAttributeTextField

                width: parent.width
                textField.placeholderText: "cn"
            }
        }

        SectionHeader
        {
            text: qsTr("Membership")
        }

        CenteredField
        {
            text: qsTr("User Membership Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: userMembershipAttributeTextField

                width: parent.width
                textField.placeholderText: "member"
            }
        }

        CenteredField
        {
            text: qsTr("Group Members Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: groupMemebersAttributeTextField

                width: parent.width
                textField.placeholderText: "memberOf"
            }
        }

        CenteredField
        {
            text: qsTr("Base membership on")
            textVCenter: true

            leftSideMargin: 180
            rightSideMargin: 0

            ColumnLayout
            {
                width: parent.width
                x: -3

                CheckBox
                {
                    id: groupMembershipCheckBox

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    Layout.topMargin: 6
                    text: qsTr("Group members")
                }

                CheckBox
                {
                    id: userMembershipCheckBox

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("User’s membership")
                }
            }
        }

        SectionHeader
        {
            text: qsTr("Misc")
        }

        RowLayout
        {
            Text
            {
                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                color: ColorTheme.colors.light16
                text: qsTr("Synchronization Timeout")
            }

            SpinBox
            {
                id: syncTimeoutSpinBox

                from: 0
                to: 30
            }

            Text
            {
                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                color: ColorTheme.colors.light16
                text: qsTr("sec")
            }
        }

        RowLayout
        {
            width: parent.width

            Text
            {
                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                color: ColorTheme.colors.light16
                text: qsTr("Proxy LDAP requests via server")
            }

            ComboBox
            {
                Layout.fillWidth: true
                model: [qsTr("Auto")]
            }
        }
    }

    function accept()
    {
        dialog.accepted()
        dialog.close()
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout

        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }
}
