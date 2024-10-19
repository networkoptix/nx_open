// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Nx.Core
import Nx.Controls
import Nx.Mobile
import Nx.Models
import Nx.Ui

import nx.vms.client.mobile

Pane
{
    id: control

    property alias systemId: informationBlock.systemId
    property alias localId: informationBlock.localId
    property alias systemName: informationBlock.systemName
    property alias cloudSystem: informationBlock.cloud
    property alias factorySystem: informationBlock.factorySystem
    property alias needDigestCloudPassword: informationBlock.needDigestCloudPassword
    property alias running: informationBlock.online
    property alias ownerDescription: informationBlock.ownerDescription
    property bool reachable: false
    property bool compatible: true
    property bool wrongCustomization: false
    property string invalidVersion

    padding: 0

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    implicitWidth: 200

    SystemHostsModel
    {
        id: hostsModel
        systemId: control.systemId
        localSystemId: control.localId
    }

    ModelDataAccessor
    {
        id: hostsModelAccessor
        model: hostsModel

        property var defaultAddress: ""

        onDataChanged: (startRow)=>
        {
            if (startRow === 0)
                updateDefaultAddress()
        }

        onRowsMoved: updateDefaultAddress()
        onCountChanged: updateDefaultAddress()

        function updateDefaultAddress()
        {
            defaultAddress = count > 0
                ? getData(0, "url-internal")
                : NxGlobals.emptyUrl()
        }
    }

    AuthenticationDataModel
    {
        id: authenticationDataModel
        localSystemId: control.localId

        readonly property bool hasData: !!defaultCredentials.user
        readonly property bool hasStoredPassword: defaultCredentials.isPasswordSaved
    }

    background: Rectangle
    {
        color:
        {
            if (control.factorySystem)
                return ColorTheme.colors.dark10

            return control.enabled ? ColorTheme.colors.dark7 : ColorTheme.colors.dark6
        }

        radius: 2

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            clip: true
            rippleSize: width * 0.8
        }
    }

    MouseArea
    {
        id: mouseArea

        parent: control
        anchors.fill: parent
        onClicked: open()
    }

    contentItem: SystemInformationBlock
    {
        id: informationBlock
        enabled: online
        address: hostsModelAccessor.defaultAddress.displayAddress()
        user: authenticationDataModel.defaultCredentials.user
        factoryDetailsText: UiMessages.factorySystemErrorText()
    }

    IconButton
    {
        /* It should not be parented by contentItem */
        parent: control

        y: 2
        z: 1
        anchors.right: parent.right
        icon.source: lp("/images/edit.png")
        visible: !cloudSystem && authenticationDataModel.hasData
        onClicked: d.openSavedSession()
    }

    IssueLabel
    {
        id: issueLabel

        visible: text !== ""

        anchors
        {
            bottom: parent.bottom
            bottomMargin: 12
            right: parent.right
            rightMargin: 12
        }

        color:
        {
            if (compatible)
                return ColorTheme.colors.dark14

            return wrongCustomization
                ? ColorTheme.colors.red_core
                : ColorTheme.colors.yellow_core
        }

        textColor: compatible ? ColorTheme.colors.light8 : ColorTheme.colors.dark4

        text:
        {
            if (!compatible)
                return wrongCustomization ?  qsTr("INCOMPATIBLE") : invalidVersion

            if (!running)
                return qsTr("OFFLINE")

            if (!reachable)
                return qsTr("UNREACHABLE")

            return ""
        }
    }

    function open()
    {
        if (factorySystem)
        {
            Workflow.openStandardDialog("", UiMessages.factorySystemErrorText())
            return
        }

        if (!compatible)
        {
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

        if (!cloudSystem)
        {
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
                            removeSavedAuth(localId, informationBlock.user)
                            d.openSavedSession(errorMessage)
                        }

                    connectionStarted = sessionManager.startSessionWithStoredCredentials(
                        hostsModelAccessor.defaultAddress,
                        NxGlobals.uuid(control.localId),
                        authenticationDataModel.defaultCredentials.user,
                        callback)
                }

                if (!connectionStarted)
                    d.openSavedSession()
            }
            else
            {
                Workflow.openDiscoveredSession(
                    systemId, localId, systemName, informationBlock.address)
            }
        }
        else if (!hostsModel.isEmpty)
        {
            sessionManager.startCloudSession(control.systemId, control.systemName)
        }
    }

    QtObject
    {
        id: d

        function openSavedSession(errorMessage)
        {
            Workflow.openSavedSession(
                systemId,
                localId,
                systemName,
                informationBlock.address,
                informationBlock.user,
                errorMessage)
        }
    }

    Keys.onEnterPressed: open()
    Keys.onReturnPressed: open()
}
