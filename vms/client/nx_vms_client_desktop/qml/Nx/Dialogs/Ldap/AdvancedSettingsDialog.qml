// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop 1.0

import "Components"
import "../UserManagement/Components"

Dialog
{
    id: dialog

    property alias isHttpDigestEnabledOnImport: digestCheckBox.checked

    property alias loginAttribute: loginAttributeTextField.text
    property alias loginAttributeAuto: loginAttributeTextField.auto
    property alias groupObjectClass: groupAttributeTextField.text
    property alias groupObjectClassAuto: groupAttributeTextField.auto
    property alias memberAttribute: userMembershipAttributeTextField.text

    property alias syncIntervalS: syncIntervalSpinBox.seconds
    property alias searchTimeoutS: searchTimeoutSpinBox.seconds

    property alias preferredSyncServer: serverComboBox.selectedServer

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

            AutoTextField
            {
                id: loginAttributeTextField
                width: parent.width
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

            AutoTextField
            {
                id: groupAttributeTextField
                width: parent.width
            }
        }

        SectionHeader
        {
            text: qsTr("Membership")
        }

        CenteredField
        {
            text: qsTr("Group Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            RowLayout
            {
                width: parent.width

                TextFieldWithValidator
                {
                    id: userMembershipAttributeTextField

                    Layout.fillWidth: true
                    textField.placeholderText: "member"
                }

                Item
                {
                    implicitWidth: loginAttributeTextField.checkBoxWidth
                    implicitHeight: userMembershipAttributeTextField.height
                }
            }
        }

        SectionHeader
        {
            text: qsTr("Misc")
        }

        CenteredField
        {
            text: qsTr("Synchronization<br>Interval")

            leftSideMargin: 180
            rightSideMargin: 0

            TimeDuration
            {
                id: syncIntervalSpinBox
            }
        }

        CenteredField
        {
            text: qsTr("Search Timeout")

            leftSideMargin: 180
            rightSideMargin: 0

            TimeDuration
            {
                id: searchTimeoutSpinBox
            }
        }

        CenteredField
        {
            text: qsTr("Proxy LDAP requests<br>via server")

            leftSideMargin: 180
            rightSideMargin: 0

            RowLayout
            {
                width: parent.width

                ServerComboBox
                {
                    id: serverComboBox

                    Layout.fillWidth: true
                }

                Item
                {
                    implicitWidth: loginAttributeTextField.checkBoxWidth
                    implicitHeight: serverComboBox.height
                }
            }
        }
    }

    function accept()
    {
        if (loginAttributeTextField.validate()
            && groupAttributeTextField.validate()
            && userMembershipAttributeTextField.validate())
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
