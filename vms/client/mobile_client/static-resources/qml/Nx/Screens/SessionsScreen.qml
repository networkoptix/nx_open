// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Mobile
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

    titleUnderlineVisible:
        rootType === OrganizationsModel.Organization
        || rootType === OrganizationsModel.Folder

    customBackHandler: () => goBack()

    readonly property bool searching: !!siteList.searchText
    readonly property bool localSitesVisible: state !== "inPartnerOrOrg"
        && (sitesTabButton.checked || !cloudUserProfileWatcher.isOrgUser)

    StackView.onActivated: forceActiveFocus()

    rightControl: IconButton
    {
        id: rightButton

        anchors.centerIn: parent

        icon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
        icon.width: 24
        icon.height: 24
    }

    centerControl:
    [
        MouseArea
        {
            anchors.fill: parent
            visible: sessionsScreen.state === "inPartnerOrOrg"
                && sessionsScreen.rootType !== OrganizationsModel.ChannelPartner
                && !searchField.visible
                && !breadcrumb.visible
            onClicked: breadcrumb.openWith(linearizationListModel.sourceRoot)
        },

        SearchEdit
        {
            id: searchField

            width: parent.width
            height: 36
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: qsTr("Search")
        }
    ]

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
                searchField.visible: true
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
                searchField.visible: true
            }
        },

        State
        {
            name: "inPartnerOrOrg"
            when: rootIndex !== NxGlobals.invalidModelIndex()

            PropertyChanges
            {
                sessionsScreen
                {
                    leftButtonImageSource:
                        "image://skin/24x24/Outline/arrow_back.svg?primary=light10"
                    onLeftButtonClicked: goBack()
                    title: !searchField.visible
                        ? accessor.getData(linearizationListModel.sourceRoot, "display")
                        : ""
                }
                rightButton
                {
                    icon.source: searchField.visible
                        ? "image://skin/24x24/Outline/close.svg?primary=light10"
                        : "image://skin/24x24/Outline/search.svg?primary=light10"
                    onClicked: searchField.visible ? endSearch() : startSearch()
                }
                systemTabs.visible: false
                searchField.visible: false
            }
        }
    ]

    Breadcrumb
    {
        id: breadcrumb

        x: siteList.x
        y: siteList.y + 8
        width: siteList.width

        onItemClicked: (nodeId) =>
        {
            goInto(organizationsModel.indexFromNodeId(nodeId))
        }

        function openWith(root)
        {
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

    OrderedSystemsModel
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

        sourceRoot: localSitesVisible
            && !sessionsScreen.searching
            && sessionsScreen.rootIndex === NxGlobals.invalidModelIndex()
                ? organizationsModel.sitesRoot
                : sessionsScreen.rootIndex
    }

    HorizontalSlide
    {
        id: siteListContainer

        anchors.fill: parent

        Rectangle
        {
            id: systemTabs

            color: ColorTheme.colors.dark8
            width: parent.width
            implicitHeight: 48
            height: visible ? implicitHeight : 0
            z: 1

            visible: !sessionsScreen.searching && !organizationsModel.topLevelLoading
                && cloudUserProfileWatcher.isOrgUser

            ButtonGroup
            {
                buttons: systemTabsRow.children
            }

            component TabButton: AbstractButton
            {
                id: tabButton

                height: 40
                width: tabText.width + 16 * 2
                checkable: true
                focusPolicy: Qt.StrongFocus

                background: Rectangle
                {
                    color: tabButton.checked ? ColorTheme.colors.dark10 : "transparent"
                    radius: 8
                }

                Text
                {
                    id: tabText
                    text: tabButton.text
                    color: tabButton.checked ? ColorTheme.colors.light4 : ColorTheme.colors.light10
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
                        && sessionsScreen.rootType !== OrganizationsModel.ChannelPartner
                }

                TabButton
                {
                    id: organizationsTabButton
                    text: qsTr("Organizations")
                }

                TabButton
                {
                    id: sitesTabButton
                    text: qsTr("Sites")
                }
            }

            function updateCheckedState()
            {
                if (organizationsModel.topLevelLoading)
                    return

                if (sessionsScreen.rootType === OrganizationsModel.ChannelPartner)
                {
                    organizationsTabButton.checked = true
                    return
                }

                if (!cloudUserProfileWatcher.isOrgUser)
                    sitesTabButton.checked = true
                else if (organizationsModel.hasChannelPartners)
                    partnersTabButton.checked = true
                else
                    organizationsTabButton.checked = true
            }

            Connections
            {
                target: organizationsModel
                function onTopLevelLoadingChanged() { systemTabs.updateCheckedState() }
            }

            Connections
            {
                target: cloudUserProfileWatcher
                function onIsOrgUserChanged() { systemTabs.updateCheckedState() }
            }

            Connections
            {
                target: sessionsScreen
                function onRootTypeChanged() { systemTabs.updateCheckedState() }
            }
        }

        width: parent.width
        height: parent.height - systemTabs.height
        y: systemTabs.height

        visible: !organizationsModel.topLevelLoading

        SiteList
        {
            id: siteList

            width: parent.width
            height: parent.height - systemTabs.height
            y: systemTabs.height

            visible: !loadingIndicator.visible
            siteModel: linearizationListModel
            hideOrgSystemsFromSites: cloudUserProfileWatcher.isOrgUser
            showOnly:
            {
                if (!systemTabsRow.visible)
                    return []

                if (partnersTabButton.checked)
                    return [OrganizationsModel.ChannelPartner]

                if (organizationsTabButton.checked)
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
                        description: qsTr("We didn't find any organizations, try contacting support")
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
                            description: qsTr("Sites in the Suspended or Shutdown state are not available"),
                        }
                    }
                    return {
                        imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10",
                        text: kNoSites,
                        description: "",
                        buttonText: qsTr("How to connect?"),
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
                        description: qsTr("We didn't find any sites on your local network, try adding servers manually or log in to your cloud account"),
                        buttonText: qsTr("Log In"),
                        buttonIconSource: "image://skin/24x24/Outline/cloud.svg?primary=dark1",
                        clickHandler: () => { Workflow.openCloudLoginScreen() }
                    }
                }

                return {
                    imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10",
                    text: kNoSites,
                    description: qsTr("We didn't find any sites on your local network, try adding servers manually"),
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

        visible: organizationsModel.topLevelLoading || currentRootLoading

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

    Component.onCompleted: resetSearch()

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

            if (isCurrentRootInvalid)
            {
                sessionsScreen.rootType = undefined
                sessionsScreen.rootIndex = NxGlobals.invalidModelIndex()
                d.listPosition.clear()
            }
        }
        function onDataChanged() { updateVisibility() }
        function onRowsInserted() { updateVisibility() }
        function onRowsRemoved() { updateVisibility() }
    }

    function goInto(newIndex)
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

        siteListContainer.slideLeft(() => {
            linearizationListModel.sourceRoot = newIndex
            sessionsScreen.rootIndex = newIndex
            sessionsScreen.rootType = rootType
            siteList.currentRoot = newIndex
        })
    }

    function goBack()
    {
        if (searchField.visible)
        {
            endSearch()
            return
        }

        siteListContainer.slideRight(() => {

            const newIndex = linearizationListModel.sourceRoot
                && linearizationListModel.sourceRoot.parent

            const currentId = accessor.getData(siteList.currentRoot, "nodeId") ?? d.kRootId
            d.listPosition.delete(currentId)

            if (newIndex.row === -1)
            {
                sessionsScreen.rootIndex = newIndex
                sessionsScreen.rootType = accessor.getData(newIndex, "type")
                linearizationListModel.sourceRoot = Qt.binding(
                    () => localSitesVisible
                        && !sessionsScreen.searching
                        && sessionsScreen.rootIndex === NxGlobals.invalidModelIndex()
                            ? organizationsModel.sitesRoot
                            : sessionsScreen.rootIndex)
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

        if (state !== "inPartnerOrOrg" && !searching)
            return

        linearizationListModel.sourceRoot = siteList.currentRoot
        searchField.visible = false
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
                windowContext.sessionManager.startCloudSession(systemId, systemName)
            else
                Workflow.openSitePlaceholderScreen(systemName)
        }
    }
}
