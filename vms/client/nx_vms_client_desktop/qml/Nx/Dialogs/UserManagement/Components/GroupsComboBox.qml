// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

MultiSelectionComboBox
{
    id: control

    property alias groupsModel: allowedParents.sourceModel

    enabledRole: "canEditMembers"
    checkedRole: "isParent"
    decorationRole: "decorationPath"
    placeholder.text: enabled ? qsTr("Select") : ("<" + qsTr("No groups") + ">")

    model: AllowedParentsModel
    {
        id: allowedParents

        property int count: 0 //< Required by MultiSelectionComboBox.

        onRowsInserted: count = rowCount()
        onRowsRemoved: count = rowCount()
        onModelReset: count = rowCount()
    }

    listView.section.property: "groupSection"
    listView.section.delegate: Rectangle
    {
        id: sectionBorder

        clip: true
        height: y > 0 ? 7 : 0 //< Hide the first item.
        width: control.listView.width
        color: ColorTheme.colors.dark13

        Rectangle
        {
            anchors
            {
                left: parent.left
                right: parent.right
                leftMargin: 6
                rightMargin: 6
                verticalCenter: parent.verticalCenter
            }
            height: 1
            color: ColorTheme.colors.dark11
        }
    }
}
