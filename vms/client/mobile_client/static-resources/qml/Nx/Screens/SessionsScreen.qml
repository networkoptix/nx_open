// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Models
import Nx.Mobile.Ui.Sheets
import Nx.Ui

import nx.vms.client.core

import "private/SessionsScreen"
import "private/FeedScreen"

AdaptiveScreen
{
    id: sessionsScreen

    objectName: "sessionsScreen"

    property var rootIndex: NxGlobals.invalidModelIndex()

    // Shared state for selected tab - controls both sidebar and horizontal tabs
    property int selectedTab: OrganizationsModel.SitesTab

    property var rootType
    readonly property bool searching: !!siteList.searchText
    readonly property bool localSitesVisible: state !== "inPartnerOrOrg"
        && (selectedTab === OrganizationsModel.SitesTab || !cloudUserProfileWatcher.isOrgUser)

    contentItem: sessionsScreenContent

    overlayItem: MouseArea
    {
        id: searchCanceller

        onPressed: (mouse) =>
        {
            mouse.accepted = false
            searchField.resetFocus()
        }
    }

    leftPanel
    {
        title: rootIndex === NxGlobals.invalidModelIndex() ? "" : qsTr("Resources")
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/resource_tree.svg?primary=dark1"
        interactive: rootIndex !== NxGlobals.invalidModelIndex()
        item:
        {
            if (LayoutController.isTabletLayout
                && !organizationsModel.topLevelLoading
                && cloudUserProfileWatcher.isOrgUser
                && (organizationsModel.hasChannelPartners || organizationsModel.hasOrganizations))
            {
                return navigationPanel
            }

            return null
        }
    }

    rightPanel
    {
        visible: false
        title: qsTr("Feed")
        color: ColorTheme.colors.dark5
        iconSource: feedStateProvider.buttonIconSource
        item:
        {
            if (rootIndex === NxGlobals.invalidModelIndex()
                || rootIndex === organizationsModel.sitesRoot)
            {
                return null
            }

            return feed
        }

        menuButton
        {
            icon.source: "image://skin/24x24/Outline/filter_list.svg?primary=light4"
            visible: !feed.empty
            indicator.visible: feed.filtered

            onClicked: feed.openFilterMenu()
        }

        onItemChanged:
        {
            if (!rightPanel.item)
                rightPanel.visible = false
        }
    }

    rightPanelButtonIndicator
    {
        text: feedStateProvider.buttonIconIndicatorText
        visible: !!feedStateProvider.buttonIconIndicatorText
    }

    customLeftControl: ToolBarButton
    {
        id: leftButton

        anchors.centerIn: parent
        icon.source: "image://skin/32x32/Outline/avatar.svg"
        icon.width: 32
        icon.height: 32
        onClicked: searchToolBar.open()
    }

    titleUnderlineVisible: !LayoutController.isTabletLayout
        && rootIndex.parent !== NxGlobals.invalidModelIndex()

    customBackHandler:
        (isEscKeyPressed) =>
        {
            if (rootIndex === NxGlobals.invalidModelIndex() && !searching && !isEscKeyPressed)
                mainWindow.close()
            else
                goBack()
        }

    StackView.onActivated:
    {
        forceActiveFocus()
        resetSearch()
    }

    customRightControl: ToolBarButton
    {
        id: rightButton

        icon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
        icon.width: 24
        icon.height: 24

        Indicator
        {
            id: rightButtonIndicator

            anchors.topMargin: (rightButton.height - rightButton.icon.height) / 2
            anchors.rightMargin: (rightButton.width - rightButton.icon.width) / 2
            visible: !!text
        }
    }

    customToolBarClickHandler: () =>
    {
        if (!LayoutController.isTabletLayout
            && sessionsScreen.state === "inPartnerOrOrg"
            && sessionsScreen.rootType !== OrganizationsModel.ChannelPartner
            && !breadcrumb.visible)
        {
            breadcrumb.openWith(linearizationListModel.sourceRoot?.parent)
        }
    }

    Connections
    {
        target: LayoutController

        function onIsTabletLayoutChanged()
        {
            if (LayoutController.isTabletLayout && sessionsScreen.contentItem === feed)
                sessionsScreen.contentItem = sessionsScreenContent
        }
    }

    states:
    [
        State
        {
            name: "watchingFeed"
            when: sessionsScreen.contentItem === feed

            PropertyChanges
            {
                sessionsScreen
                {
                    title: "%1 - %2"
                        .arg(feed.title)
                        .arg(accessor.getData(linearizationListModel.sourceRoot, "display"))
                }

                leftButton
                {
                    icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=light10"
                    icon.width: 24
                    icon.height: 24
                    onClicked: sessionsScreen.contentItem = sessionsScreenContent
                }

                rightButton
                {
                    icon.source: "image://skin/24x24/Outline/filter_list.svg?primary=light4"
                    visible: !feed.empty
                    onClicked: feed.openFilterMenu()
                }

                rightButtonIndicator
                {
                    visible: feed.filtered
                    text: ""
                }
            }
        },

        State
        {
            name: "loggedIn"
            when: appContext.cloudStatusWatcher.status !== CloudStatusWatcher.LoggedOut
                && rootIndex === NxGlobals.invalidModelIndex()

            PropertyChanges
            {
                leftButton
                {
                    imageSource: cloudUserProfileWatcher.avatarUrl
                    onClicked: Workflow.openCloudSummaryScreen(cloudUserProfileWatcher)
                }
                sessionsScreen
                {
                    title: qsTr("Welcome, %1!").arg(cloudUserProfileWatcher.fullName)
                    leftPanel.visible: true
                }
                rightButton
                {
                    icon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
                    onClicked: Workflow.openSettingsScreen(/*push*/ true)
                }
                systemTabs.visible: !LayoutController.isTabletLayout
                    && !sessionsScreen.searching
                    && !loadingIndicator.visible
                    && cloudUserProfileWatcher.isOrgUser
                    && (organizationsModel.hasChannelPartners
                        || organizationsModel.hasOrganizations)
            }
        },

        State
        {
            name: "loggedOut"
            when: appContext.cloudStatusWatcher.status === CloudStatusWatcher.LoggedOut
                && rootIndex === NxGlobals.invalidModelIndex()

            PropertyChanges
            {
                leftButton
                {
                    icon.source: ""
                    onClicked: Workflow.openCloudLoginScreen()
                }
                sessionsScreen
                {
                    title: ""
                }
                rightButton
                {
                    icon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
                    onClicked: Workflow.openSettingsScreen(/*push*/ true)
                }
                systemTabs.visible: false
            }
        },

        State
        {
            name: "inPartnerOrOrg"
            when: rootIndex !== NxGlobals.invalidModelIndex()
                && rootIndex !== organizationsModel.sitesRoot

            PropertyChanges
            {
                leftButton
                {
                    icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=light10"
                    icon.width: 24
                    icon.height: 24
                    onClicked: goBack()
                }
                sessionsScreen
                {
                    title: searchField.displayText
                        ? ""
                        : accessor.getData(linearizationListModel.sourceRoot, "display")
                }

                rightButton
                {
                    visible: !emptyListPlaceholder.visible && !LayoutController.isTabletLayout
                    icon.source: feedStateProvider.buttonIconSource
                    onClicked: sessionsScreen.contentItem = feed
                }

                rightButtonIndicator.text: feedStateProvider.buttonIconIndicatorText

                systemTabs.visible: false
            }
        }
    ]

    SystemsModel
    {
        id: systemsModel
    }

    OrganizationsModel
    {
        id: organizationsModel

        statusWatcher: cloudUserProfileWatcher.statusWatcher
        systemsModel: systemsModel
    }

    ModelDataAccessor
    {
        id: accessor

        model: organizationsModel
    }

    LinearizationListModel
    {
        id: linearizationListModel

        sourceModel: organizationsModel
        autoExpandAll: sessionsScreen.searching
        onAutoExpandAllChanged:
        {
            if (!autoExpandAll)
                collapseAll()
        }

        Component.onCompleted: resetSourceRoot()

        function resetSourceRoot()
        {
            linearizationListModel.sourceRoot = Qt.binding(
                () => (localSitesVisible
                    && !sessionsScreen.searching
                    && sessionsScreen.rootIndex === NxGlobals.invalidModelIndex())
                        ? organizationsModel.sitesRoot
                        : sessionsScreen.rootIndex)
        }
    }

    Feed
    {
        id: feed

        feedState: FeedStateProvider
        {
            id: feedStateProvider

            iconColor: LayoutController.isTabletLayout ? "dark1" : "light4"
            cloudSystemIds: organizationsModel.childSystemIds(sessionsScreen.rootIndex)
        }
    }

    Item
    {
        visible: false

        component TabButton: AbstractButton
        {
            id: tabButton

            implicitHeight: 40
            implicitWidth: tabText.implicitWidth + leftPadding + rightPadding
            topPadding: 10
            leftPadding: 16
            rightPadding: 16
            bottomPadding: 10
            checkable: true
            focusPolicy: Qt.StrongFocus

            background: Rectangle
            {
                color: tabButton.checked ? ColorTheme.colors.dark10 : "transparent"
                radius: 8
            }

            contentItem: Text
            {
                id: tabText
                text: tabButton.text
                color: tabButton.checked ? ColorTheme.colors.light4 : ColorTheme.colors.light10
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: LayoutController.isTabletLayout ? Text.AlignLeft : Text.AlignHCenter
            }
        }

        ButtonGroup
        {
            id: tabGroup
        }

        TabButton
        {
            id: partnersTabButton
            text: qsTr("Partners")
            visible: organizationsModel.hasChannelPartners
            ButtonGroup.group: tabGroup
            checked: sessionsScreen.selectedTab === OrganizationsModel.ChannelPartnersTab
            onClicked: sessionsScreen.selectedTab = OrganizationsModel.ChannelPartnersTab
        }

        TabButton
        {
            id: organizationsTabButton
            text: qsTr("Organizations")
            visible: organizationsModel.hasOrganizations
            ButtonGroup.group: tabGroup
            checked: sessionsScreen.selectedTab === OrganizationsModel.OrganizationsTab
            onClicked: sessionsScreen.selectedTab = OrganizationsModel.OrganizationsTab
        }

        TabButton
        {
            id: sitesTabButton
            text: qsTr("Sites")
            ButtonGroup.group: tabGroup
            checked: sessionsScreen.selectedTab === OrganizationsModel.SitesTab
            onClicked: sessionsScreen.selectedTab = OrganizationsModel.SitesTab
        }
    }

    NavigationPanel
    {
        id: navigationPanel

        hasChannelPartners: organizationsModel.hasChannelPartners
        hasOrganizations: organizationsModel.hasOrganizations
        currentTab: sessionsScreen.selectedTab
        onTabSelected: (selectedTabValue) => sessionsScreen.selectedTab = selectedTabValue

        organizationsModel: organizationsModel
        searchText: siteList.searchText
        selectedOrganizationIndex: {
            if (sessionsScreen.state !== "inPartnerOrOrg")
                return null

            var idx = sessionsScreen.rootIndex
            while (idx && idx !== NxGlobals.invalidModelIndex())
            {
                const t = accessor.getData(idx, "type")
                if (t === OrganizationsModel.Organization)
                    return idx

                idx = idx.parent
            }
            return null
        }

        onOrganizationNodeClicked: (nodeId) =>
        {
            const nodeIndex = organizationsModel.indexFromNodeId(nodeId)
            if (nodeIndex !== NxGlobals.invalidModelIndex())
            {
                const nodeType = accessor.getData(nodeIndex, "type")
                if (nodeType === OrganizationsModel.System)
                    sessionsScreen.openSystem(nodeIndex)
                else
                    goInto(nodeIndex)
            }
        }
    }

    Item
    {
        id: sessionsScreenContent

        HorizontalSlide
        {
            id: siteListContainer

            anchors.fill: parent
            visible: !loadingIndicator.visible

            Breadcrumb
            {
                id: breadcrumb

                width: parent.width

                onItemClicked: (nodeId) =>
                {
                    goBack(organizationsModel.indexFromNodeId(nodeId))
                }

                function openWith(root)
                {
                    if (root === NxGlobals.invalidModelIndex())
                        return

                    let data = []

                    for (let node = root; node && node.row !== -1; node = node.parent)
                    {
                        data.push({
                            name: accessor.getData(node, "display"),
                            nodeId: accessor.getData(node, "nodeId")
                        })
                    }

                    model = data.reverse()
                    open()
                }

                Connections
                {
                    target: LayoutController

                    function onIsTabletLayoutChanged()
                    {
                        if (LayoutController.isTabletLayout && breadcrumb.visible)
                            breadcrumb.close()
                    }
                }
            }

            ColumnLayout
            {
                anchors.fill: parent
                anchors.topMargin: 20 + (breadcrumb.visible ? breadcrumb.height : 0)
                spacing: 20

                SearchEdit
                {
                    id: searchField

                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20

                    implicitHeight: 36
                    placeholderText: qsTr("Search")
                }

                Rectangle
                {
                    id: systemTabs

                    Layout.fillWidth: true
                    color: ColorTheme.colors.dark8
                    implicitHeight: 48
                    z: 1
                    visible: !LayoutController.isTabletLayout
                        && !sessionsScreen.searching
                        && !organizationsModel.topLevelLoading
                        && cloudUserProfileWatcher.isOrgUser
                        && (organizationsModel.hasChannelPartners || organizationsModel.hasOrganizations)

                    readonly property var selectedTab:
                    {
                        if (tabGroup.checkedButton === partnersTabButton)
                            return OrganizationsModel.ChannelPartnersTab
                        if (tabGroup.checkedButton === organizationsTabButton)
                            return OrganizationsModel.OrganizationsTab
                        if (tabGroup.checkedButton === sitesTabButton)
                            return OrganizationsModel.SitesTab

                        return OrganizationsModel.SitesTab
                    }

                    Row
                    {
                        id: systemTabsRow
                        spacing: 0
                        x: 20
                        y: 4

                        height: visible ? implicitHeight : 0

                        LayoutItemProxy
                        {
                            target: partnersTabButton
                            visible: organizationsModel.hasChannelPartners
                        }
                        LayoutItemProxy
                        {
                            target: organizationsTabButton
                            visible: organizationsModel.hasOrganizations
                        }
                        LayoutItemProxy
                        {
                            target: sitesTabButton
                        }
                    }

                    function updateCheckedState(force)
                    {
                        if (organizationsModel.topLevelLoading && !force)
                            return

                        if (!systemTabs.visible && !force)
                        {
                            // If no tabs are going to be visible, select the Sites tab.
                            if (!organizationsModel.hasChannelPartners
                                && !organizationsModel.hasOrganizations)
                            {
                                sessionsScreen.selectedTab = OrganizationsModel.SitesTab
                            }

                            return
                        }

                        // Avoid switching tabs when selected tab is already visible.
                        const currentTabVisible =
                            (sessionsScreen.selectedTab === OrganizationsModel.ChannelPartnersTab && partnersTabButton.visible) ||
                            (sessionsScreen.selectedTab === OrganizationsModel.OrganizationsTab && organizationsTabButton.visible) ||
                            (sessionsScreen.selectedTab === OrganizationsModel.SitesTab)

                        // Avoid switching tabs when selected tab is already visible.
                        if (currentTabVisible && !force)
                            return

                        // Select appropriate tab based on the current state.
                        if (cloudUserProfileWatcher.isOrgUser)
                        {
                            if (organizationsModel.hasChannelPartners)
                            {
                                sessionsScreen.selectedTab = OrganizationsModel.ChannelPartnersTab
                                return
                            }
                            if (organizationsModel.hasOrganizations)
                            {
                                sessionsScreen.selectedTab = OrganizationsModel.OrganizationsTab
                                return
                            }
                        }

                        sessionsScreen.selectedTab = OrganizationsModel.SitesTab
                    }

                    Connections
                    {
                        target: organizationsModel
                        function onTopLevelLoadingChanged() { systemTabs.updateCheckedState() }
                        function onHasChannelPartnersChanged()
                        {
                            systemTabs.updateCheckedState(/*force*/ organizationsModel.hasChannelPartners)
                        }
                        function onHasOrganizationsChanged()
                        {
                            systemTabs.updateCheckedState(/*force*/ organizationsModel.hasOrganizations)
                        }
                    }

                    Connections
                    {
                        target: cloudUserProfileWatcher
                        function onIsOrgUserChanged() { systemTabs.updateCheckedState(/*force*/ true) }
                    }

                    Connections
                    {
                        target: sessionsScreen
                        function onRootTypeChanged() { systemTabs.updateCheckedState() }
                    }
                }


                SiteList
                {
                    id: siteList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !loadingIndicator.visible
                    siteModel: linearizationListModel
                    hideOrgSystemsFromSites: cloudUserProfileWatcher.isOrgUser
                    currentTab: sessionsScreen.selectedTab
                    clip: true
                    showOnly:
                    {
                        if (!systemTabsRow.visible)
                            return []

                        if (currentTab === OrganizationsModel.ChannelPartnersTab)
                            return [OrganizationsModel.ChannelPartner]

                        if (currentTab === OrganizationsModel.OrganizationsTab)
                            return [OrganizationsModel.Organization]

                        return []
                    }

                    PropertyUpdateFilter on searchText
                    {
                        source: searchField.displayText
                        minimumIntervalMs: 250
                    }

                    onItemClicked: (nodeId, systemId) =>
                    {
                        if (!nodeId)
                        {
                            sessionsScreen.openSystem(organizationsModel.indexFromSystemId(systemId))
                            return
                        }

                        // Synchronize tree selection with right panel
                        if (nodeId)
                            navigationPanel.selectNodeById(nodeId)

                        const current = organizationsModel.indexFromNodeId(nodeId)
                        if (accessor.getData(current, "type") === OrganizationsModel.System)
                            sessionsScreen.openSystem(current)
                        else
                            goInto(organizationsModel.indexFromNodeId(nodeId))
                    }
                }
            }

            Placeholder
            {
                id: emptyListPlaceholder
                objectName: "emptyListPlaceholder"

                visible: siteList.count == 0 && !loadingIndicator.visible

                property bool isAdministrator: false

                readonly property var textData:
                {
                    const kNoSites = qsTr("No Sites")

                    if (sessionsScreen.searching)
                    {
                        return {
                            imageSource: "image://skin/64x64/Outline/notfound.svg?primary=light10",
                            text: qsTr("Nothing found"),
                            description: qsTr("Try changing the search parameters")
                        }
                    }
                    if ((sessionsScreen.state === "loggedIn"
                            && organizationsTabButton.checked
                            && cloudUserProfileWatcher.isOrgUser)
                        || (sessionsScreen.state === "inPartnerOrOrg"
                            && rootType == OrganizationsModel.ChannelPartner))
                    {
                        return {
                            imageSource: "image://skin/64x64/Outline/no_organization.svg?primary=light10",
                            text: qsTr("No Organizations"),
                            description:
                                qsTr("Create an organization in the Cloud Portal to access it here")
                        }
                    }
                    if (sessionsScreen.state === "inPartnerOrOrg"
                        && rootType == OrganizationsModel.Organization)
                    {
                        if (accessor.getData(sessionsScreen.rootIndex, "isAccessDenied") ?? false)
                        {
                            return {
                                imageSource: "image://skin/64x64/Outline/no_access.svg?primary=light10",
                                text: qsTr("Access to Resources Denied"),
                                description: qsTr("The resources in this organization are not "
                                    + "available to your permission group"),
                            }
                        }

                        return {
                            imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10",
                            text: kNoSites,
                            description: qsTr("Connect a site to the organization to access it here"),
                            buttonText: isAdministrator ? qsTr("How to connect?") : "",
                            buttonIconSource: "image://skin/24x24/Outline/cloud.svg?primary=dark1",
                            clickHandler: () => { cloudConnectionHelp.open() }
                        }
                    }
                    if (sessionsScreen.state === "inPartnerOrOrg"
                        && rootType == OrganizationsModel.Folder)
                    {
                        return {
                            imageSource: "image://skin/64x64/Outline/openfolder.svg?primary=light10",
                            text: qsTr("Folder is empty"),
                            description: ""
                        }
                    }
                    if (sessionsScreen.state === "loggedOut")
                    {
                        return {
                            imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10",
                            text: kNoSites,
                            description: qsTr("No accessible sites were found. " +
                                "Log in into the cloud account or connect to a local server"),
                            buttonText: qsTr("Log In"),
                            buttonIconSource: "image://skin/24x24/Outline/cloud.svg?primary=dark1",
                            clickHandler: () => { Workflow.openCloudLoginScreen() }
                        }
                    }

                    return {
                        imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10",
                        text: kNoSites,
                        description: qsTr("No accessible sites were found. " +
                            "Request access to existing sites or connect to a local server"),
                    }
                }

                anchors.centerIn: parent
                imageSource: textData.imageSource
                text: textData.text
                description: textData.description
                buttonText: textData.buttonText || ""
                buttonIcon.source: textData.buttonIconSource || ""
                onButtonClicked: textData.clickHandler()
            }

            IconButton
            {
                id: customConnectionButton

                visible: localSitesVisible && !searching && !organizationsModel.topLevelLoading

                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.bottomMargin: 20
                anchors.rightMargin: 20

                icon.source: "image://skin/24x24/Outline/plus.svg?primary=dark1"
                icon.width: 24
                icon.height: 24
                padding: 16

                TapHandler
                {
                    onTapped: siteConnectionSheet.connectToSite()
                }

                background: Rectangle
                {
                    color: ColorTheme.colors.light10
                    radius: 16
                }
            }
        }

        Skeleton
        {
            id: loadingIndicator

            anchors.fill: parent
            visible: loading && appContext.cloudStatusWatcher.status !== CloudStatusWatcher.Offline

            property bool currentRootLoading: false
            property bool waitingForLastOpened: appGlobalState.lastOpenedNodeId !== NxGlobals.uuid("")
                && appGlobalState.lastOpenedNodeId !== undefined
            readonly property bool loading: (organizationsModel.topLevelLoading || currentRootLoading || waitingForLastOpened)
                && appContext.cloudStatusWatcher.status !== CloudStatusWatcher.Offline
                || !organizationsModel.firstLoadAttemptFinished

            component MaskItem: Rectangle
            {
                radius: 8
                color: "white"
                antialiasing: false
            }

            Row
            {
                id: systemTabsRowSkeleton

                visible: sessionsScreen.state !== "inPartnerOrOrg"

                x: 20
                y: 4
                spacing: 4

                MaskItem
                {
                    width: 120
                    height: 40
                }

                MaskItem
                {
                    width: 120
                    height: 40
                }
            }

            Flow
            {
                anchors.fill: parent
                topPadding: sessionsScreen.state !== "inPartnerOrOrg" ? 64 : 16
                leftPadding: 20
                spacing: siteList.spacing
                Repeater
                {
                    model: 4

                    delegate: MaskItem
                    {
                        width: siteList.cellWidth
                        height: 116
                    }
                }
            }
        }
    }

    CloudConnectionHelpSheet
    {
        id: cloudConnectionHelp
    }

    SiteConnectionSheet
    {
        id: siteConnectionSheet
    }

    Component.onCompleted:
    {
        resetSearch()
    }

    Connections
    {
        target: organizationsModel

        function onFullTreeLoaded()
        {
            if (sessionsScreen.rootIndex === NxGlobals.invalidModelIndex()
                && appGlobalState.lastOpenedNodeId !== NxGlobals.uuid("")
                && appGlobalState.lastOpenedNodeId !== undefined)
            {
                const nodeIndex =
                    organizationsModel.indexFromNodeId(appGlobalState.lastOpenedNodeId)
                appGlobalState.lastOpenedNodeId = NxGlobals.uuid("")
                if (nodeIndex !== NxGlobals.invalidModelIndex())
                    goInto(nodeIndex.parent, /*animate*/ false)
            }
        }
    }

    QtObject
    {
        id: d
        property var listPosition: new Map()
        readonly property var kRootId: NxGlobals.uuid("")
    }

    Connections
    {
        target: accessor
        function updateVisibility()
        {
            const isCurrentRootInvalid =
                sessionsScreen.rootIndex === NxGlobals.invalidModelIndex()
                || NxGlobals.fromPersistent(sessionsScreen.rootIndex)
                    === NxGlobals.invalidModelIndex()

            loadingIndicator.currentRootLoading =
                !isCurrentRootInvalid
                && accessor.getData(sessionsScreen.rootIndex, "isLoading")

            emptyListPlaceholder.isAdministrator = Qt.binding(
                () => sessionsScreen.rootIndex !== NxGlobals.invalidModelIndex()
                    && accessor.getData(sessionsScreen.rootIndex, "isAdministrator"))

            if (isCurrentRootInvalid)
            {
                sessionsScreen.rootType = undefined

                // IMPORTANT:
                // This property is used by many bindings and state conditions.
                // Re-assigning it, even with the same value, forces synchronous
                // re-evaluation of those bindings and may be expensive.
                // During model population `updateVisibility()` is triggered very
                // frequently (e.g. on rowsInserted), so we must avoid redundant
                // assignments to prevent UI stalls.
                if (sessionsScreen.rootIndex !== NxGlobals.invalidModelIndex())
                    sessionsScreen.rootIndex = NxGlobals.invalidModelIndex()

                linearizationListModel.resetSourceRoot()
                siteList.currentRoot = linearizationListModel.sourceRoot

                d.listPosition.clear()
            }
        }
        function onDataChanged() { updateVisibility() }
        function onRowsInserted() { updateVisibility() }
        function onRowsRemoved() { updateVisibility() }
    }

    function goInto(newIndex, animate = true)
    {
        endSearch()

        const currentId = accessor.getData(siteList.currentRoot, "nodeId") ?? d.kRootId
        d.listPosition.set(currentId, siteList.contentY)

        const rootType = accessor.getData(newIndex, "type")

        if (rootType === OrganizationsModel.System)
        {
            openSystem(newIndex)
            return
        }

        newIndex = NxGlobals.toPersistent(newIndex)

        // Synchronize tree selection when navigating into a node
        const nodeId = accessor.getData(newIndex, "nodeId")
        if (nodeId)
            navigationPanel.selectNodeById(nodeId)

        const update = () =>
            {
                linearizationListModel.sourceRoot = newIndex
                sessionsScreen.rootIndex = newIndex
                sessionsScreen.rootType = rootType
                siteList.currentRoot = newIndex
            }

        if (animate)
            siteListContainer.slideLeft(update)
        else
            update()
    }

    function goBack(index)
    {
        if (searchField.displayText)
        {
            endSearch()
            return
        }

        if (index === sessionsScreen.rootIndex)
            return

        siteListContainer.slideRight(() =>
        {
            const newIndex = index ||
                linearizationListModel.sourceRoot && linearizationListModel.sourceRoot.parent

            const currentId = accessor.getData(siteList.currentRoot, "nodeId") ?? d.kRootId
            d.listPosition.delete(currentId)

            if (newIndex.row === -1)
            {
                if (sessionsScreen.rootType === OrganizationsModel.ChannelPartner)
                    partnersTabButton.checked = true
                else if (sessionsScreen.rootType === OrganizationsModel.Organization)
                    organizationsTabButton.checked = true
                else
                    sitesTabButton.checked = true

                sessionsScreen.rootIndex = newIndex
                sessionsScreen.rootType = accessor.getData(newIndex, "type")
                linearizationListModel.resetSourceRoot()
                siteList.currentRoot = linearizationListModel.sourceRoot
            }
            else
            {
                sessionsScreen.rootIndex = NxGlobals.toPersistent(newIndex)
                sessionsScreen.rootType = accessor.getData(newIndex, "type")
                linearizationListModel.sourceRoot = newIndex
                siteList.currentRoot = newIndex
            }
            const newId = accessor.getData(newIndex, "nodeId") ?? d.kRootId
            if (newId)
                navigationPanel.selectNodeById(newId)

            siteList.contentY = d.listPosition.get(newId) ?? 0
        })
    }

    function startSearch()
    {
        if (searchField.displayText)
            return

        siteList.currentRoot = linearizationListModel.sourceRoot
        linearizationListModel.sourceRoot = Qt.binding(
            () => searching ? NxGlobals.invalidModelIndex() : siteList.currentRoot)
        searchField.visible = true
        searchField.forceActiveFocus()
    }

    function endSearch()
    {
        if (!searchField.displayText)
            return

        if (state === "inPartnerOrOrg")
            linearizationListModel.sourceRoot = siteList.currentRoot

        searchField.clear()
        searchField.resetFocus()
        siteList.positionViewAtBeginning()
    }

    function resetSearch()
    {
        searchField.clear()
        siteList.positionViewAtBeginning()
    }

    function openSystem(current)
    {
        appGlobalState.lastOpenedNodeId = accessor.getData(current, "nodeId")
        const isFactory = accessor.getData(current, "isFactorySystem")
        if (isFactory)
        {
            Workflow.openStandardDialog("", UiMessages.factorySystemErrorText())
            return
        }

        const systemName = accessor.getData(current, "display")

        const isCompatible = accessor.getData(current, "isCompatibleToMobileClient")
        if (!isCompatible)
        {
            const wrongCustomization = accessor.getData(current, "wrongCustomization")
            if (wrongCustomization)
            {
                const incompatibleServerErrorText = UiMessages.incompatibleServerErrorText()
                Workflow.openSessionsScreenWithWarning(systemName, incompatibleServerErrorText)
            }
            else
            {
                const tooOldErrorText = UiMessages.tooOldServerErrorText()
                Workflow.openSessionsScreenWithWarning(systemName, tooOldErrorText)
            }

            return
        }

        const systemId = accessor.getData(current, "systemId")
        const isCloudSystem = accessor.getData(current, "isCloudSystem")

        if (!isCloudSystem)
        {
            const localId = accessor.getData(current, "localId")
                siteConnectionSheet.connectToSite(systemName, systemId, localId)
        }
        else if (!siteConnectionSheet.hostsModel.isEmpty)
        {
            const isAccessible = !accessor.getData(current, "isSaasSuspended")
                && !accessor.getData(current, "isSaasShutDown")

            if (isAccessible)
            {
                windowContext.sessionManager.startCloudSession(systemId, systemName)
            }
            else
            {
                appGlobalState.lastOpenedNodeId = NxGlobals.uuid("")
                Workflow.openSitePlaceholderScreen(systemName)
            }
        }
    }
}
