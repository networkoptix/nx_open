// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "../Components"

Item
{
    id: control

    property alias name: groupNameTextField.text
    property alias description: descriptionTextArea.text

    property var self

    function validate()
    {
        return groupNameTextField.validate()
    }

    Column
    {
        spacing: 8
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16

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
                validateFunc: self ? self.validateName : null
            }
        }

        CenteredField
        {
            text: qsTr("Description")

            TextArea
            {
                id: descriptionTextArea
                width: parent.width
                height: 60
            }
        }
    }
}
