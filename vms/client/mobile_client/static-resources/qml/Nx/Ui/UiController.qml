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
        target: windowContext.deprecatedUiController

        function onConnectToServerScreenRequested(host, user, password, operationId)
        {
            Workflow.openConnectToServerScreen(host, user, password, operationId)
        }

        function onResourcesScreenRequested(filterIds)
        {
            Workflow.openResourcesScreen(windowContext.sessionManager.systemName, filterIds)
        }

        function onVideoScreenRequested(cameraResource, timestamp)
        {
            Workflow.openVideoScreen(cameraResource, undefined, timestamp)
        }

        function onSessionsScreenRequested()
        {
            Workflow.openSessionsScreen()
        }
    }

    Binding
    {
        target: windowContext.deprecatedUiController
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
                case "securitySettingsScreen":
                    return Controller.SecuritySettingsScreen
                case "interfaceSettingsScreen":
                    return Controller.InterfaceSettingsScreen
                case "performanceSettingsScreen":
                    return Controller.PerformanceSettingsScreen
                case "appInfoScreen":
                    return Controller.AppInfoScreen
                case "betaFeaturesScreen":
                    return Controller.BetaFeaturesScreen
                case "pushExpertModeScreen":
                    return Controller.PushExpertSettingsScreen
                case "loginToCloudScreen":
                    return Controller.LoginToCloudScreen
                case "digestLoginToCloudScreen":
                    return Controller.DigestLoginToCloudScreen
                case "cameraSettingsScreen":
                    return Controller.CameraSettingsScreen
                case "eventSearchScreen":
                    return Controller.EventSearchScreen
                case "eventDetailsScreen":
                    return Controller.DetailsScreen
                case "eventSearchMenuScreen":
                    return Controller.EventSearchMenuScreen
                case "feedScreen":
                    return Controller.FeedScreen
                case "menuScreen":
                    return Controller.MenuScreen
                default:
                    return Controller.UnknownScreen
            }
        }
    }
}
