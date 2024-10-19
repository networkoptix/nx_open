// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Mobile 1.0
import Nx.Ui 1.0

import nx.vms.client.mobile 1.0

NxObject
{
    property StackView stackView

    Connections
    {
        target: uiController

        function onConnectToServerScreenRequested(host, user, password, operationId)
        {
            Workflow.openConnectToServerScreen(host, user, password, operationId)
        }

        function onResourcesScreenRequested(filterIds)
        {
            Workflow.openResourcesScreen(sessionManager.systemName, filterIds)
        }

        function onVideoScreenRequested(cameraResource, timestamp)
        {
            Workflow.openVideoScreen(cameraResource, undefined, undefined, undefined, timestamp)
        }

        function onSessionsScreenRequested()
        {
            Workflow.openSessionsScreen()
        }
    }

    Binding
    {
        target: uiController
        property: "currentScreen"
        value: d.screenByName(stackView && stackView.currentItem
            ? stackView.currentItem.objectName
            : "")
    }

    NxObject
    {
        id: d

        function screenByName(screenName)
        {
            switch (screenName)
            {
                case "sessionsScreen":
                    return Controller.SessionsScreen
                case "customConnectionScreen":
                    return Controller.CustomConnectionScreen
                case "resourcesScreen":
                    return Controller.ResourcesScreen
                case "videoScreen":
                    return Controller.VideoScreen
                case "settingsScreen":
                    return Controller.SettingsScreen
                case "pushExpertModeScreen":
                    return Controller.PushExpertSettingsScreen
                case "loginToCloudScreen":
                    return Controller.LoginToCloudScreen
                case "digestLoginToCloudScreen":
                    return Controller.DigestLoginToCloudScreen
                case "cameraSettingsScreen":
                    return Controller.CameraSettingsScreen
                case "betaFeaturesScreen":
                    return Controller.BetaFeaturesScreen
                default:
                    return Controller.UnknownScreen
            }
        }
    }
}
