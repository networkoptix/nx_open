// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Shapes 1.15
import QtQuick.Window 2.15

import Qt5Compat.GraphicalEffects

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "../Components"

Item
{
    id: control

    property var groupId
    property alias name: groupNameTextField.text
    property bool nameEditable: true
    property bool descriptionEditable: true
    property bool isLdap: false
    property bool isPredefined: false
    property alias description: descriptionTextArea.text
    property var groups: []
    property int userCount: 0
    property int groupCount: 0
    property bool deleteAvailable: true
    property bool continuousSync: true
    property var editingContext
    property bool nameIsUnique: true

    property alias model: groupsComboBox.model
    property alias parentGroupsEditable: groupsComboBox.enabled

    property var self

    function validate()
    {
        return groupNameTextField.validate()
    }

    signal deleteRequested()
    signal moreGroupsClicked()

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        Rectangle
        {
            color: ColorTheme.colors.dark8
            Layout.fillWidth: true
            height: 103

            Image
            {
                id: groupTypeIcon
                x: 24
                y: 24
                width: 64
                height: 64
                source: control.isLdap
                    ? "image://svg/skin/user_settings/group_type_ldap.svg"
                    : (control.isPredefined
                        ? "image://svg/skin/user_settings/group_type_built_in.svg"
                        : "image://svg/skin/user_settings/group_type_local.svg")
                sourceSize: Qt.size(width, height)
            }

            EditableLabel
            {
                id: groupNameTextField

                enabled: control.nameEditable && control.enabled
                validateFunc: self ? (text) => self.validateName(text) : null

                anchors.left: groupTypeIcon.right
                anchors.leftMargin: 24
                anchors.verticalCenter: groupTypeIcon.verticalCenter

                anchors.right: parent.right
                anchors.rightMargin: 16

                Connections
                {
                    target: control.model

                    function onUserIdChanged() { groupNameTextField.forceFinishEdit() }
                }
            }

            Row
            {
                anchors.bottom: groupTypeIcon.bottom
                anchors.right: parent.right
                anchors.rightMargin: 20
                spacing: 12

                TextButton
                {
                    icon.source: "image://svg/skin/user_settings/user_delete.svg"
                    icon.width: 12
                    icon.height: 14
                    spacing: 8
                    visible: control.deleteAvailable
                    text: qsTr("Delete")
                    onClicked: control.deleteRequested()
                }
            }
        }

        Rectangle
        {
            color: ColorTheme.colors.dark6

            Layout.fillWidth: true
            height: 1
        }

        Scrollable
        {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true

            contentItem: Column
            {
                x: 16
                width: scroll.width - 16 * 2

                spacing: 8

                SectionHeader
                {
                    text: qsTr("Info")
                }

                CenteredField
                {
                    text: qsTr("Description")

                    TextAreaWithScroll
                    {
                        id: descriptionTextArea

                        width: parent.width
                        height: 64
                        readOnly: !control.descriptionEditable
                        wrapMode: TextEdit.Wrap
                    }
                }

                CenteredField
                {
                    text: qsTr("Permission Groups")

                    visible: !control.isPredefined

                    GroupsComboBox
                    {
                        id: groupsComboBox

                        width: parent.width
                    }
                }

                SectionHeader
                {
                    Layout.fillWidth: true
                    text: qsTr("Members")
                }

                SummaryTable
                {
                    Layout.topMargin: 8
                    model: {
                        return [
                            { "label": qsTr("Users"), "text": `${control.userCount}` },
                            { "label": qsTr("Groups"), "text": `${control.groupCount}` }
                        ]
                    }
                }
            }
        }

        DialogBanner
        {
            id: bannerLdapContinousSyncDisabled

            style: DialogBanner.Style.Warning
            closeable: true
            watchToReopen: control.groupId
            visible: control.isLdap && !control.continuousSync && !closed
            Layout.fillWidth: true

            text: qsTr("When Continuous Sync is disabled, groups do not synchronize automatically. "
                + "To update this group, initiate a manual sync.")
        }

        DialogBanner
        {
            style: DialogBanner.Style.Info
            visible: !control.nameIsUnique && !closed
            closeable: true
            Layout.fillWidth: true

            text: qsTr("Another group with the same name exists in the system."+
                " It is recommended to assign unique names to the groups.")
        }
    }
}
