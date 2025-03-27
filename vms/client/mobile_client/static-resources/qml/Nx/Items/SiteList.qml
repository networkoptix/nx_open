// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Models
import Nx.Items

ListView
{
    id: siteList

    property alias siteModel: filterModel.sourceModel
    property alias hideOrgSystemsFromSites: filterModel.hideOrgSystemsFromSites

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
        property real sectionHeight:
            12 + Math.ceil(textMetrics.height) + (siteList.cellsInRow == 1 ? 10 : 12)
        width: parent.width
        readonly property real systemTabsHeight: 48
        height: 16 + (!!currentSearchRegExp ? (systemTabsHeight - sectionHeight) : 0)
        onHeightChanged:
        {
            // Avoids delegates re-creation during component construction.
            Qt.callLater(() => siteList.positionViewAtBeginning())
        }
    }

    footer: Item
    {
        width: parent.width
        height: 16
    }

    TextMetrics
    {
        id: textMetrics
        font.pixelSize: 14
        text: qsTr("Organizations") + qsTr("Folders") + qsTr("Sites")
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
    section.delegate: Item
    {
        width: parent.width
        height: 12 + sectionText.height + (siteList.cellsInRow == 1 ? 10 : 12)
        Text
        {
            id: sectionText
            color: ColorTheme.colors.light16
            text: section
            font.pixelSize: 14
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
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
                isSaas: !modelData.isSaasUninitialized
                    && (organizationsModel.channelPartner != NxGlobals.uuid(""))
                needDigestCloudPassword: !modelData.isOauthSupported && !hasDigestCloudPassword
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

    Keys.onPressed:
    {
        // TODO: #dklychkov Implement transparent navigation to the footer item.
        if (event.key == Qt.Key_C)
        {
            Workflow.openNewSessionScreen()
            event.accepted = true
        }
    }
}
