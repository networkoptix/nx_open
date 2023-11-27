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

    property bool continuousSyncEnabled: true

    property alias isHttpDigestEnabledOnImport: digestCheckBox.checked
    property bool isHttpDigestEnabledOnImportInitial

    property alias loginAttribute: loginAttributeTextField.text
    property alias loginAttributeAuto: loginAttributeTextField.auto
    property alias groupObjectClass: groupAttributeTextField.text
    property alias groupObjectClassAuto: groupAttributeTextField.auto
    property alias memberAttribute: userMembershipAttributeTextField.text
    property alias memberAttributeAuto: userMembershipAttributeTextField.auto

    property alias syncIntervalS: syncIntervalSpinBox.seconds
    property alias searchTimeoutS: searchTimeoutSpinBox.seconds

    property alias preferredSyncServer: serverComboBox.selectedServer

    property alias continuousSync: syncComboBox.selectedSync

    modality: Qt.ApplicationModal

    minimumWidth: 540
    minimumHeight: content.height + buttonBox.implicitHeight + banner.height + 16

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
            text: qsTr("General")
        }

        CenteredField
        {
            text: qsTr("Synchronize Users")

            leftSideMargin: 180
            rightSideMargin: 0

            RowLayout
            {
                width: parent.width

                SyncComboBox
                {
                    id: syncComboBox

                    Layout.fillWidth: true

                    syncEnabled: dialog.continuousSyncEnabled
                }

                Item
                {
                    implicitWidth: loginAttributeTextField.checkBoxWidth
                    implicitHeight: serverComboBox.height
                }
            }
        }

        CenteredField
        {
            text: qsTr("Sync Interval")

            leftSideMargin: 180
            rightSideMargin: 0

            TimeDuration
            {
                id: syncIntervalSpinBox

                enabled: syncComboBox.selectedSync != LdapSettings.Sync.disabled
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
            text: qsTr("Proxy LDAP requests %1 via server", "%1 is a line break").arg("<br>")

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
                text: qsTr("Allow insecure (digest) authentication for imported users")
                wrapMode: Text.WordWrap
            }
        }

        SectionHeader
        {
            text: qsTr("Groups")
        }

        CenteredField
        {
            text: "objectClass"

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
            text: qsTr("Group Members Attribute")

            leftSideMargin: 180
            rightSideMargin: 0

            AutoTextField
            {
                id: userMembershipAttributeTextField
                width: parent.width
            }
        }
    }

    DialogBanner
    {
        id: banner

        width: parent.width
        anchors.bottom: buttonBox.top

        style: DialogBanner.Style.Info
        text: digestCheckBox.checked
            ? qsTr("To enable digest authentication for LDAP users that are already added to the "
                + "VMS database, you need to enable it directly in these users' settings.")
            : qsTr("To disable digest authentication for LDAP users that are already added to the "
                + "VMS database, you need to disable it directly in these users' settings.")
        visible: isHttpDigestEnabledOnImport !== isHttpDigestEnabledOnImportInitial
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
