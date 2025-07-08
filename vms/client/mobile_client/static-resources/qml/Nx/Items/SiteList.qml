// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Models
import Nx.Items

ListView
{
    id: siteList

    property alias siteModel: filterModel.sourceModel
    property alias hideOrgSystemsFromSites: filterModel.hideOrgSystemsFromSites
    property alias showOnly: filterModel.showOnly

    property alias currentRoot: filterModel.currentRoot
    property alias currentTab: filterModel.currentTab

    property var organizationsModel: siteModel.sourceModel

    signal itemClicked(var nodeId, string systemId)
    signal itemEditClicked(var nodeId, string systemId)

    spacing: cellsInRow == 1 ? 8 : 12

    readonly property real horizontalSpacing: 12
    readonly property real maxCellWidth: 488 + horizontalSpacing
    property int cellsInRow: Math.max(1, Math.ceil(width / maxCellWidth))
    property real cellWidth: (
        width - leftMargin - rightMargin - (cellsInRow - 1) * horizontalSpacing) / cellsInRow

    model: SectionColumnModel
    {
        columns: siteList.cellsInRow
        sourceModel: OrganizationsFilterModel
        {
            id: filterModel
        }
        sectionProperty: "section"
    }

    leftMargin: 20
    rightMargin: 20

    header: Item
    {
        width: parent.width
        height: 16
    }

    footer: Item
    {
        width: parent.width
        height: 16
    }

    property string searchText: ""
    onSearchTextChanged:
    {
        // For text highlight.
        siteList.currentSearchRegExp = searchText
            ? new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(searchText)})`, 'i')
            : ""

        filterModel.setFilterRegularExpression(siteList.currentSearchRegExp)
        if (!searchText.length)
            positionViewAtBeginning()
    }

    property var currentSearchRegExp: null
    readonly property color highlightColor: ColorTheme.colors.yellow_d1

    function highlightMatchingText(text)
    {
        if (currentSearchRegExp)
            return NxGlobals.highlightMatch(text, currentSearchRegExp, highlightColor)

        return NxGlobals.toHtmlEscaped(text)
    }

    section.property: currentSearchRegExp ? "section" : ""
    section.criteria: ViewSection.FullString
    section.delegate: ColumnLayout
    {
        readonly property var sectionTextParts: section.split("\n")

        readonly property string sectionTitle: sectionTextParts.length > 1
            ? sectionTextParts[0]
            : ""
        readonly property string sectionDescription: sectionTextParts.length > 1
            ? sectionTextParts[1]
            : sectionTextParts[0]

        readonly property string placeholderDescription: sectionTextParts.length > 2
            ? sectionTextParts[2]
            : ""

        width: parent.width
        spacing: 12

        Placeholder
        {
            id: noResultsPlaceholder

            visible: placeholderDescription
            Layout.alignment: Qt.AlignHCenter

            imageSource: "image://skin/64x64/Outline/notfound.svg?primary=light10"
            text: qsTr("Nothing Found")
            description: qsTr("Try changing the search parameters")
        }

        Text
        {
            id: titleText

            Layout.topMargin: 24
            Layout.fillWidth: true

            color: ColorTheme.colors.light4
            text: sectionTitle
            visible: text
            font.pixelSize: 16
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideMiddle
        }

        Text
        {
            id: descriptionText

            Layout.topMargin: titleText.visible ? 0 : (siteList.cellsInRow == 1 ? 16 : 24)
            Layout.bottomMargin: 8
            Layout.fillWidth: true

            color: ColorTheme.colors.light16
            text: sectionDescription
            font.pixelSize: 14
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideMiddle
        }
    }

    delegate: Row
    {
        spacing: siteList.horizontalSpacing
        property var rowData: model.data

        Repeater
        {
            model: rowData

            delegate: SiteListItem
            {
                width: siteList.cellWidth

                type: modelData.type
                title: highlightMatchingText(modelData.display)
                text: highlightMatchingText(modelData.display)
                counter: modelData.systemCount >= 0 ? modelData.systemCount : -1

                systemId: modelData.systemId
                localId: modelData.localId || ""
                cloudSystem: modelData.isCloudSystem || false
                factorySystem: modelData.isFactorySystem || false
                needDigestCloudPassword: !modelData.isOauthSupported
                    && !appContext.credentialsHelper.hasDigestCloudPassword
                isSaas: !modelData.isSaasUninitialized
                    && (organizationsModel.channelPartner != NxGlobals.uuid(""))
                saasSuspended: modelData.isSaasSuspended ?? false
                saasShutDown: modelData.isSaasShutDown ?? false
                isFromSites: modelData.isFromSites ?? false
                ownerDescription: cloudSystem ? modelData.ownerDescription : ""
                running: modelData.isOnline || false
                reachable: modelData.isReachable || false
                compatible: (modelData.isCompatibleToMobileClient || modelData.isFactorySystem) || false
                wrongCustomization: modelData.wrongCustomization ? modelData.wrongCustomization : false
                invalidVersion: modelData.wrongVersion ? modelData.wrongVersion.toString() : ""
                editEnabled: modelData.type == OrganizationsModel.System && !cloudSystem

                onClicked: siteList.itemClicked(modelData.nodeId, modelData.systemId)
                onEditClicked: siteList.itemEditClicked(modelData.nodeId, modelData.systemId)
            }
        }
    }

    highlightMoveDuration: 0

    focus: false
}
