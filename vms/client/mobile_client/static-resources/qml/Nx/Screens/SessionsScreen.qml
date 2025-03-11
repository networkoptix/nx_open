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
import Nx.Ui

import nx.vms.client.core

Page
{
    id: sessionsScreen
    objectName: "sessionsScreen"

    leftButtonImageSource: cloudStatusWatcher.status == CloudStatusWatcher.Online
        ? cloudUserProfileWatcher.avatarUrl
        : ""
    leftButtonIcon.source: "image://skin/32x32/Outline/avatar.svg"
    leftButtonIcon.width: 32
    leftButtonIcon.height: 32

    rightButtonIcon.source: "image://skin/24x24/Outline/settings.svg?primary=light10"
    rightButtonIcon.width: 24
    rightButtonIcon.height: 24

    onLeftButtonClicked:
    {
        var showSummary = cloudStatusWatcher.status == CloudStatusWatcher.Online
            || cloudStatusWatcher.status == CloudStatusWatcher.Offline

        if (!showSummary)
        {
            Workflow.openCloudLoginScreen()
            return
        }

        Workflow.openCloudSummaryScreen(cloudUserProfileWatcher)
    }
    onRightButtonClicked: Workflow.openSettingsScreen()

    readonly property bool searching: !!siteList.searchText
    readonly property bool localSitesVisible:
        sitesTabButton.checked || !cloudUserProfileWatcher.isOrgUser

    StackView.onActivated: forceActiveFocus()

    titleControls:
    [
        SearchEdit
        {
            id: searchField

            width: parent.width
            height: 36
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: qsTr("Search")

            onDisplayTextChanged: siteList.searchText = displayText
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

    Rectangle
    {
        id: systemTabs

        color: ColorTheme.colors.dark8
        width: parent.width
        implicitHeight: 48
        height: visible ? implicitHeight : 0
        z: 1

        visible: !sessionsScreen.searching && !organizationsModel.channelPartnerLoading
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

            TabButton
            {
                id: organizationsTabButton
                checked: true
                text: qsTr("Organizations")
            }

            TabButton
            {
                id: sitesTabButton
                text: qsTr("Sites")
            }
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
        channelPartner: cloudUserProfileWatcher.channelPartner

        signal systemOpened(var current)

        onSystemOpened: (current) => sessionsScreen.openSystem(current)
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
        sourceRoot: localSitesVisible && !sessionsScreen.searching
            ? organizationsModel.sitesRoot
            : NxGlobals.invalidModelIndex()
    }

    Placeholder
    {
        id: searchNotFoundPlaceholder

        visible: sessionsScreen.searching && siteList.count == 0

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2
        imageSource: "image://skin/64x64/Outline/notfound.svg?primary=light10"
        text: qsTr("Nothing found")
        description: qsTr("Try changing the search parameters")
    }

    Placeholder
    {
        id: noOrganizationPlaceholder

        visible: !sessionsScreen.searching && siteList.count == 0 && organizationsTabButton.checked
            && !organizationsModel.channelPartnerLoading && cloudUserProfileWatcher.isOrgUser

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2
        imageSource: "image://skin/64x64/Outline/no_organization.svg?primary=light10"
        text: qsTr("No Organizations")
        description: qsTr("We didn't find any organizations, try contacting support")
    }

    Placeholder
    {
        id: noSitesPlaceholder

        visible: !sessionsScreen.searching && siteList.count == 0
            && !organizationsModel.channelPartnerLoading && localSitesVisible

        z: 1

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2

        readonly property bool isLoggedOut:
            cloudStatusWatcher.status == CloudStatusWatcher.LoggedOut

        imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10"
        text: qsTr("No Sites Found")
        description: isLoggedOut
            ? qsTr("We didn't find any sites on your local network, try adding servers manually or log in to your cloud account")
            : qsTr("We didn't find any sites on your local network, try adding servers manually")
        buttonText: isLoggedOut ? qsTr("Log In") : ""
        buttonIcon.source: "image://skin/24x24/Outline/cloud.svg?primary=dark1"
        onButtonClicked: Workflow.openCloudLoginScreen()
    }

    SiteList
    {
        id: siteList

        width: parent.width
        height: parent.height - systemTabs.height
        y: systemTabs.height

        visible: !organizationsModel.channelPartnerLoading
        siteModel: linearizationListModel
        hideOrgSystemsFromSites: cloudUserProfileWatcher.isOrgUser

        onItemClicked: (nodeId) =>
        {
            let current = organizationsModel.indexFromId(nodeId)

            if (accessor.getData(current, "type") != OrganizationsModel.System)
            {
                Workflow.openOrganizationScreen(organizationsModel, current)
                return
            }

            sessionsScreen.openSystem(current)
        }
    }

    IconButton
    {
        id: customConnectionButton

        visible: localSitesVisible && !searching

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
            onTapped: Workflow.openNewSessionScreen()
        }

        background: Rectangle
        {
            color: ColorTheme.colors.light10
            radius: 16
        }
    }

    Skeleton
    {
        anchors.fill: parent

        visible: organizationsModel.channelPartnerLoading

        component MaskItem: Rectangle
        {
            radius: 8
            color: "white"
            antialiasing: false
        }

        Row
        {
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
            topPadding: 64
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

    function resetSearch()
    {
        searchField.clear()
        siteList.positionViewAtBeginning()
    }

    onVisibleChanged:
    {
        if (visible)
            resetSearch()
    }

    Component.onCompleted: resetSearch()

    AuthenticationDataModel
    {
        id: authenticationDataModel

        readonly property bool hasData: !!defaultCredentials.user
        readonly property bool hasStoredPassword: defaultCredentials.isPasswordSaved
    }

    SystemHostsModel
    {
        id: hostsModel
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
            authenticationDataModel.localSystemId = localId

            hostsModel.systemId = systemId
            hostsModel.localSystemId = localId
            const defaultAddress = hostsModel.rowCount() > 0
                ? NxGlobals.modelData(hostsModel.index(0, 0), "url-internal")
                : NxGlobals.emptyUrl()

            if (authenticationDataModel.hasData)
            {
                let connectionStarted = false
                if (authenticationDataModel.hasStoredPassword)
                {
                    const callback =
                        function(connectionStatus, errorMessage)
                        {
                            if (connectionStatus !== SessionManager.LocalSessionExiredStatus)
                                return

                            Workflow.openSessionsScreen() //< Resets connection state/preloader.
                            removeSavedAuth(
                                localId, authenticationDataModel.defaultCredentials.user)
                            openSavedSession(
                                systemId, localId, systemName, defaultAddress, errorMessage)
                        }

                    connectionStarted = sessionManager.startSessionWithStoredCredentials(
                        defaultAddress,
                        NxGlobals.uuid(localId),
                        authenticationDataModel.defaultCredentials.user,
                        callback)
                }

                if (!connectionStarted)
                    openSavedSession(systemId, localId, systemName, defaultAddress)
            }
            else
            {
                Workflow.openDiscoveredSession(
                    systemId, localId, systemName, defaultAddress.displayAddress())
            }
        }
        else if (!hostsModel.isEmpty)
        {
            sessionManager.startCloudSession(systemId, systemName)
        }
    }

    function openSavedSession(systemId, localId, systemName, defaultAddress, errorMessage)
    {
        Workflow.openSavedSession(
            systemId,
            localId,
            systemName,
            defaultAddress.displayAddress(),
            authenticationDataModel.defaultCredentials.user,
            errorMessage)
    }
}
