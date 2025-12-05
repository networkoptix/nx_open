// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Models

/**
 * Navigation list component with vertical menu items (Partners, Organizations, Sites).
 */
Item
{
    id: navList

    property bool hasChannelPartners: false
    property bool hasOrganizations: false
    property int currentTab: OrganizationsModel.SitesTab

    signal tabSelected(var tab)

    Column
    {
        anchors.fill: parent
        anchors.topMargin: 20
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        spacing: 0

        // Partners
        NavigationItem
        {
            width: parent.width
            text: qsTr("Partners")
            visible: navList.hasChannelPartners
            checked: navList.currentTab === OrganizationsModel.ChannelPartnersTab
            onClicked: navList.tabSelected(OrganizationsModel.ChannelPartnersTab)
        }

        // Organizations
        NavigationItem
        {
            width: parent.width
            text: qsTr("Organizations")
            visible: navList.hasOrganizations
            checked: navList.currentTab === OrganizationsModel.OrganizationsTab
            onClicked: navList.tabSelected(OrganizationsModel.OrganizationsTab)
        }

        // Sites
        NavigationItem
        {
            width: parent.width
            text: qsTr("Sites")
            checked: navList.currentTab === OrganizationsModel.SitesTab
            onClicked: navList.tabSelected(OrganizationsModel.SitesTab)
        }
    }

    // Navigation item component
    component NavigationItem: AbstractButton
    {
        id: navItem

        implicitHeight: 40
        focusPolicy: Qt.StrongFocus

        background: Rectangle
        {
            color: navItem.checked ? ColorTheme.colors.dark10 :
                   navItem.hovered ? ColorTheme.colors.dark8 : "transparent"
            radius: 8
        }

        contentItem: Text
        {
            text: navItem.text
            color: navItem.checked ? ColorTheme.colors.light4 : ColorTheme.colors.light10
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
            leftPadding: 16
            rightPadding: 16
            topPadding: 9.5
            bottomPadding: 9.5
        }
    }
}
