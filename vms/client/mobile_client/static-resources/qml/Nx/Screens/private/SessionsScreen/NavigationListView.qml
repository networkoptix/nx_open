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
    property int partnerCount: 0
    property int organizationCount: 0
    property int siteCount: 0
    property bool showCount: false
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

        NavigationButton
        {
            width: parent.width
            text: qsTr("Partners") + (navList.showCount ? " (%1)".arg(navList.partnerCount) : "")
            visible: navList.hasChannelPartners
            checked: navList.currentTab === OrganizationsModel.ChannelPartnersTab
            onClicked: navList.tabSelected(OrganizationsModel.ChannelPartnersTab)
        }

        NavigationButton
        {
            width: parent.width
            text: qsTr("Organizations") + (navList.showCount ? " (%1)".arg(navList.organizationCount) : "")
            visible: navList.hasOrganizations
            checked: navList.currentTab === OrganizationsModel.OrganizationsTab
            onClicked: navList.tabSelected(OrganizationsModel.OrganizationsTab)
        }

        NavigationButton
        {
            width: parent.width
            text: qsTr("Sites") + (navList.showCount ? " (%1)".arg(navList.siteCount) : "")
            checked: navList.currentTab === OrganizationsModel.SitesTab
            onClicked: navList.tabSelected(OrganizationsModel.SitesTab)
        }
    }
}
