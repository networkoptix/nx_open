// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Shapes 1.15
import QtQuick.Window 2.15

import QtGraphicalEffects 1.0

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "../Components"

Item
{
    id: control

    property alias name: groupNameTextField.text
    property bool nameEditable: true
    property bool isLdap: false
    property alias description: descriptionTextArea.text
    property var groups: []
    property int userCount: 0
    property int groupCount: 0
    property bool deleteAvailable: true
    property var editingContext
    property alias globalPermissions: accessSummaryModel.globalPermissions
    property alias sharedResources: accessSummaryModel.sharedResources

    property var self

    function validate()
    {
        return groupNameTextField.validate()
    }

    signal deleteRequested()
    signal groupClicked(var id)
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
                    : "image://svg/skin/user_settings/group_type_local.svg"
                sourceSize: Qt.size(width, height)
            }

            EditableLabel
            {
                id: groupNameTextField

                enabled: control.nameEditable
                validateFunc: self ? self.validateName : null

                anchors.left: groupTypeIcon.right
                anchors.leftMargin: 24
                anchors.verticalCenter: groupTypeIcon.verticalCenter

                anchors.right: parent.right
                anchors.rightMargin: 16
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
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column
            {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                spacing: 8

                SectionHeader
                {
                    text: qsTr("Info")
                }

                CenteredField
                {
                    text: qsTr("Description")

                    TextArea
                    {
                        id: descriptionTextArea
                        width: parent.width
                        height: 60
                        enabled: control.nameEditable
                        wrapMode: TextEdit.Wrap
                    }
                }

                SectionHeader
                {
                    text: qsTr("Groups")
                }

                GroupsFlow
                {
                    id: groupsFlow

                    width: parent.width

                    rowLimit: 2
                    items: control.groups

                    onGroupClicked: (id) => { control.groupClicked(id) }
                    onMoreClicked: control.moreGroupsClicked()
                }

                Text
                {
                    visible: control.groups.length === 0
                    font: Qt.font({pixelSize: 14, weight: Font.Normal})
                    color: ColorTheme.colors.light16
                    text: qsTr("Not a member of any group")
                }

                SectionHeader
                {
                    text: qsTr("Custom Permissions")
                }

                PermissionSummary
                {
                    Layout.topMargin: 8

                    model: CustomAccessSummaryModel
                    {
                        id: accessSummaryModel
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
    }
}
