// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "../Components"

Item
{
    id: control

    property alias name: groupNameTextField.text
    property alias description: descriptionTextArea.text

    property var self
    property alias model: groupsComboBox.model

    function validate()
    {
        return groupNameTextField.validate()
    }

    Scrollable
    {
        id: scroll

        anchors.fill: parent

        contentItem: Column
        {
            spacing: 8
            x: 16
            width: scroll.width - 16 * 2

            SectionHeader
            {
                text: qsTr("New Group")
            }

            CenteredField
            {
                text: qsTr("Name")

                TextFieldWithValidator
                {
                    id: groupNameTextField
                    width: parent.width
                    validateFunc: self ? (text) => self.validateName(text) : null
                }
            }

            CenteredField
            {
                text: qsTr("Description")

                TextAreaWithScroll
                {
                    id: descriptionTextArea

                    width: parent.width
                    height: 64
                    wrapMode: TextEdit.Wrap

                    textArea.KeyNavigation.priority: KeyNavigation.BeforeItem
                    textArea.KeyNavigation.tab: groupsComboBox
                    textArea.KeyNavigation.backtab: groupNameTextField
                }
            }

            CenteredField
            {
                text: qsTr("Permission Groups")

                toolTipText: dialog.self ? dialog.self.kToolTipText : ""
                GroupsComboBox
                {
                    id: groupsComboBox

                    width: parent.width
                }
            }
        }
    }
}
