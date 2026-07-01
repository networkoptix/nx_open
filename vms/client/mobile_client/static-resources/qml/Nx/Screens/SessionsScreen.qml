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

Page
{
    id: sessionsScreen
    objectName: "sessionsScreen"

    leftButtonIcon.source: "image://skin/32x32/Outline/avatar.svg"
    leftButtonIcon.width: state === "inPartnerOrOrg" ? 24 : 32
    leftButtonIcon.height: state === "inPartnerOrOrg" ? 24 : 32

    property var rootIndex: NxGlobals.invalidModelIndex()
    property var rootType

    titleUnderlineVisible: rootIndex.parent !== NxGlobals.invalidModelIndex()

    customBackHandler:
        (isEscKeyPressed) =>
        {
            if (rootIndex === NxGlobals.invalidModelIndex() && !searching && !isEscKeyPressed)
                mainWindow.close()
            else
                goBack()
        }

    // Shared state for the selected tab; drives the tab buttons and the site list.
    property int selectedTab: OrganizationsModel.SitesTab

    readonly property bool searching: !!siteList.searchText
    readonly property bool localSitesVisible: state !== "inPartnerOrOrg"
        && (selectedTab === OrganizationsModel.SitesTab || !cloudUserProfileWatcher.isOrgUser)

    StackView.onActivated: forceActiveFocus()

    rightControl: IconButton
    {
        id: rightButton

        anchors.centerIn: parent

        icon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
        icon.width: 24
        icon.height: 24

        Indicator
        {
            id: rightButtonIndicator

            anchors.margins: rightButton.padding * 2
            visible: !!text
        }
    }

    centerControl:
    [
        MouseArea
        {
            anchors.fill: parent
            visible: sessionsScreen.state === "inPartnerOrOrg"
                && sessionsScreen.rootType !== OrganizationsModel.ChannelPartner
                && !breadcrumb.visible
            onClicked: breadcrumb.openWith(linearizationListModel.sourceRoot?.parent)
        },

        LayoutItemProxy
        {
            id: topSearchField

            target: searchField
            visible: false
            width: parent.width
            height: 36
            anchors.verticalCenter: parent.verticalCenter
        }
    ]

    SearchEdit
    {
        id: searchField

        placeholderText: qsTr("Search")
    }

    MouseArea
    {
        id: searchCanceller

        z: 1
        anchors.fill: parent

        onPressed: (mouse) =>
        {
            mouse.accepted = false
            searchField.resetFocus()
        }
    }

    states:
    [
        State
        {
            name: "loggedIn"
            when: appContext.cloudStatusWatcher.status !== CloudStatusWatcher.LoggedOut
                && rootIndex === NxGlobals.invalidModelIndex()

            PropertyChanges
            {
                sessionsScreen
                {
                    leftButtonImageSource: cloudUserProfileWatcher.avatarUrl
                    onLeftButtonClicked:
                        Workflow.openCloudSummaryScreen(cloudUserProfileWatcher)
                    title: ""
                }
                rightButton
                {
                    icon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
                    onClicked: Workflow.openSettingsScreen()
                }
                systemTabs.visible: !sessionsScreen.searching
                    && !loadingIndicator.visible
                    && cloudUserProfileWatcher.isOrgUser
                    && (organizationsModel.hasChannelPartners
                        || organizationsModel.hasOrganizations)

                topSearchField.visible: true
            }
        },

        State
        {
            name: "loggedOut"
            when: appContext.cloudStatusWatcher.status === CloudStatusWatcher.LoggedOut
                && rootIndex === NxGlobals.invalidModelIndex()

            PropertyChanges
            {
                sessionsScreen
                {
                    leftButtonImageSource: ""
                    onLeftButtonClicked: Workflow.openCloudLoginScreen()
                    title: ""
                }
                rightButton
                {
                    icon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
                    onClicked: Workflow.openSettingsScreen()
                }
                systemTabs.visible: false
                topSearchField.visible: true
            }
        },

        State
        {
            name: "inPartnerOrOrg"
            when: rootIndex !== NxGlobals.invalidModelIndex()
                && rootIndex !== organizationsModel.sitesRoot

            PropertyChanges
            {
                sessionsScreen
                {
                    leftButtonImageSource:
                        "image://skin/24x24/Outline/arrow_back.svg?primary=light10"
                    onLeftButtonClicked: goBack()
                    title: accessor.getData(sessionsScreen.rootIndex, "display")
                }

                rightButton
                {
                    visible: !emptyListPlaceholder.visible
                    icon.source: feedStateProvider.buttonIconSource
                    onClicked: Workflow.openFeedScreen(feedStateProvider)
                }

                rightButtonIndicator.text: feedStateProvider.buttonIconIndicatorText

                systemTabs.visible: false
                contentSearchField.visible: true
            }
        }
    ]

    Breadcrumb
    {
        id: breadcrumb

        x: siteList.x
        y: contentSearchField.y
        width: siteList.width

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
    }

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
                () =>
                {
                    if (sessionsScreen.searching
                        && sessionsScreen.rootIndex !== NxGlobals.invalidModelIndex()
                        && sessionsScreen.rootIndex !== organizationsModel.sitesRoot)
                    {
                        return NxGlobals.invalidModelIndex()
                    }

                    return (localSitesVisible
                        && !sessionsScreen.searching
                        && sessionsScreen.rootIndex === NxGlobals.invalidModelIndex())
                            ? organizationsModel.sitesRoot
                            : sessionsScreen.rootIndex
                })
        }
    }

    FeedStateProvider
    {
        id: feedStateProvider

        cloudSystemIds: organizationsModel.childSystemIds(sessionsScreen.rootIndex)
    }

    HorizontalSlide
    {
        id: siteListContainer

        anchors.fill: parent

        RectangularShadow
        {
            anchors.fill: systemTabs
            blur: 24
            spread: 0
            cached: true
            color: Qt.rgba(0, 0, 0, 0.7)
            z: 1
            visible: systemTabs.visible
        }

        LayoutItemProxy
        {
            id: contentSearchField

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 20

            target: searchField
            height: visible ? 36 : 0
            visible: false
        }

        Rectangle
        {
            id: systemTabs

            color: ColorTheme.colors.dark8
            width: parent.width
            implicitHeight: 48
            height: visible ? implicitHeight : 0
            y: contentSearchField.y + contentSearchField.height
            z: 1

            visible: !sessionsScreen.searching && !organizationsModel.topLevelLoading
                && cloudUserProfileWatcher.isOrgUser
                && (organizationsModel.hasChannelPartners
                    || organizationsModel.hasOrganizations)

            component TabButton: AbstractButton
            {
                id: tabButton

                property bool highlighted: false

                height: 40
                width: tabText.width + 16 * 2
                focusPolicy: Qt.StrongFocus

                background: Rectangle
                {
                    color: tabButton.highlighted ? ColorTheme.colors.dark10 : "transparent"
                    radius: 8
                }

                Text
                {
                    id: tabText
                    text: tabButton.text
                    color: tabButton.highlighted ? ColorTheme.colors.light4 : ColorTheme.colors.light10
                    font.pixelSize: 14
                    anchors.centerIn: parent
                }
            }

            Row
            {
                id: systemTabsRow
                spacing: 0
                x: 20
                y: 4

                height: visible ? implicitHeight : 0

                TabButton
                {
                    id: partnersTabButton
                    text: qsTr("Partners")
                    visible: organizationsModel.hasChannelPartners
                    highlighted: sessionsScreen.selectedTab === OrganizationsModel.ChannelPartnersTab
                    onClicked: sessionsScreen.selectTabByUser(OrganizationsModel.ChannelPartnersTab)
                }

                TabButton
                {
                    id: organizationsTabButton
                    text: qsTr("Organizations")
                    visible: organizationsModel.hasOrganizations
                    highlighted: sessionsScreen.selectedTab === OrganizationsModel.OrganizationsTab
                    onClicked: sessionsScreen.selectTabByUser(OrganizationsModel.OrganizationsTab)
                }

                TabButton
                {
                    id: sitesTabButton
                    text: qsTr("Sites")
                    highlighted: sessionsScreen.selectedTab === OrganizationsModel.SitesTab
                    onClicked: sessionsScreen.selectTabByUser(OrganizationsModel.SitesTab)
                }
            }

            function updateSelectedTab()
            {
                if (organizationsModel.topLevelLoading)
                    return

                // Restore the tab the user explicitly selected last, if it is still available.
                const savedTab = appGlobalState.lastSelectedOrgTab
                if (savedTab !== -1)
                {
                    const savedAvailable =
                        (savedTab === OrganizationsModel.ChannelPartnersTab
                            && organizationsModel.hasChannelPartners)
                        || (savedTab === OrganizationsModel.OrganizationsTab
                            && organizationsModel.hasOrganizations)
                        || (savedTab === OrganizationsModel.SitesTab)

                    if (savedAvailable)
                    {
                        sessionsScreen.selectedTab = savedTab
                        return
                    }
                }

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
                function onTopLevelLoadingChanged() { systemTabs.updateSelectedTab() }
                function onHasChannelPartnersChanged() { systemTabs.updateSelectedTab() }
                function onHasOrganizationsChanged() { systemTabs.updateSelectedTab() }
            }

            Connections
            {
                target: cloudUserProfileWatcher
                function onIsOrgUserChanged() { systemTabs.updateSelectedTab() }
            }

            Connections
            {
                target: appContext.cloudStatusWatcher
                function onStatusChanged()
                {
                    if (appContext.cloudStatusWatcher.status === CloudStatusWatcher.LoggedOut)
                        appGlobalState.lastSelectedOrgTab = -1
                }
            }

            Connections
            {
                target: sessionsScreen
                function onRootTypeChanged() { systemTabs.updateSelectedTab() }
            }
        }

        width: parent.width
        height: parent.height - systemTabs.height
        y: systemTabs.height

        visible: !organizationsModel.topLevelLoading
            || appContext.cloudStatusWatcher.status === CloudStatusWatcher.Offline

        SiteList
        {
            id: siteList

            width: parent.width
            height: parent.height - systemTabs.height
            y: systemTabs.y + systemTabs.height
            clip: true

            visible: !loadingIndicator.visible
            siteModel: linearizationListModel
            hideOrgSystemsFromSites: cloudUserProfileWatcher.isOrgUser
            currentTab: sessionsScreen.selectedTab
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

                const current = organizationsModel.indexFromNodeId(nodeId)

                if (accessor.getData(current, "type") === OrganizationsModel.System)
                    sessionsScreen.openSystem(current)
                else
                    goInto(organizationsModel.indexFromNodeId(nodeId))
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
                        && sessionsScreen.selectedTab === OrganizationsModel.OrganizationsTab
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
            anchors.verticalCenterOffset: -header.height / 2
            imageSource: textData.imageSource
            text: textData.text
            description: textData.description
            buttonText: textData.buttonText || ""
            buttonIcon.source: textData.buttonIconSource || ""
            onButtonClicked: textData.clickHandler()
        }
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

    Skeleton
    {
        id: loadingIndicator

        anchors.fill: parent

        property bool currentRootLoading: false
        property bool waitingForLastOpened: appGlobalState.lastOpenedNodeId !== NxGlobals.uuid("")
            && appGlobalState.lastOpenedNodeId !== undefined

        visible: (organizationsModel.topLevelLoading || currentRootLoading || waitingForLastOpened)
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

    CloudConnectionHelpSheet
    {
        id: cloudConnectionHelp
    }

    SiteConnectionSheet
    {
        id: siteConnectionSheet
    }

    onVisibleChanged:
    {
        if (visible)
            resetSearch()
    }

    Component.onCompleted:
    {
        resetSearch()
        // Restore the last user-selected tab and the page within it after the screen is
        // recreated (e.g. on disconnect), covering the case where the organizations model is
        // already loaded and emits no signal.
        systemTabs.updateSelectedTab()
        restoreLastOpenedLocation()
    }

    Connections
    {
        target: organizationsModel

        function onFullTreeLoaded() { sessionsScreen.restoreLastOpenedLocation() }
    }

    Connections
    {
        target: windowContext.sessionManager

        function onSessionStoppedManually() { sessionsScreen.restoreLastOpenedLocation() }
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

    Connections
    {
        target: siteList

        function onSearchTextChanged()
        {
            if (sessionsScreen.state !== "inPartnerOrOrg")
                return

            const sourceRoot = sessionsScreen.searching
                ? NxGlobals.invalidModelIndex()
                : sessionsScreen.rootIndex

            linearizationListModel.sourceRoot = sourceRoot
            siteList.currentRoot = sessionsScreen.rootIndex
        }
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
        if (searchField.text)
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
                    sessionsScreen.selectTabByUser(OrganizationsModel.ChannelPartnersTab)
                else if (sessionsScreen.rootType === OrganizationsModel.Organization)
                    sessionsScreen.selectTabByUser(OrganizationsModel.OrganizationsTab)
                else
                    sessionsScreen.selectTabByUser(OrganizationsModel.SitesTab)

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
            siteList.contentY = d.listPosition.get(newId) ?? 0
        })
    }

    function startSearch()
    {
        if (searchField.visible)
            return

        siteList.currentRoot = linearizationListModel.sourceRoot
        linearizationListModel.sourceRoot = Qt.binding(
            () => searching ? NxGlobals.invalidModelIndex() : siteList.currentRoot)
        searchField.visible = true
        searchField.forceActiveFocus()
    }

    function endSearch()
    {
        if (!searchField.visible)
            return

        if (state === "inPartnerOrOrg")
        {
            linearizationListModel.sourceRoot = sessionsScreen.rootIndex
            siteList.currentRoot = sessionsScreen.rootIndex
        }

        searchField.clear()
        searchField.resetFocus()
        siteList.positionViewAtBeginning()
    }

    function resetSearch()
    {
        searchField.clear()
        siteList.positionViewAtBeginning()
    }

    function selectTabByUser(tab)
    {
        selectedTab = tab
        appGlobalState.lastSelectedOrgTab = tab
    }

    // Return to the page within the current tab (folder / organization / partner) that contained
    // the last opened site, so coming back from a session lands on that page instead of the tab
    // root. Idempotent: a no-op unless we are at the top level and a last opened site is pending.
    function restoreLastOpenedLocation()
    {
        if (sessionsScreen.rootIndex !== NxGlobals.invalidModelIndex())
            return

        if (appGlobalState.lastOpenedNodeId === NxGlobals.uuid("")
            || appGlobalState.lastOpenedNodeId === undefined)
        {
            return
        }

        const nodeIndex = organizationsModel.indexFromNodeId(appGlobalState.lastOpenedNodeId)
        if (nodeIndex !== NxGlobals.invalidModelIndex())
        {
            appGlobalState.lastOpenedNodeId = NxGlobals.uuid("")
            goInto(nodeIndex.parent, /*animate*/ false)
        }
        else if (organizationsModel.firstLoadAttemptFinished)
        {
            // The tree has finished loading but the site is gone. Clear the marker so the loading
            // skeleton (waitingForLastOpened) does not persist.
            appGlobalState.lastOpenedNodeId = NxGlobals.uuid("")
        }
        // Otherwise the tree is still loading; onFullTreeLoaded() will retry the restore.
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
