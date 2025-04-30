// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Settings
import Nx.Ui

import nx.vms.client.core

BaseSettingsScreen
{
    id: settingsScreen

    objectName: "settingsScreen"

    title: qsTr("Settings")

    LabeledSwitch
    {

        width: parent.width
        text: qsTr("Interface")
        icon: "image://skin/24x24/Solid/interface.svg?primary=light1"
        showIndicator: false
        onClicked: Workflow.openInterfaceSettingsScreen()
    }

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Security")
        icon: "image://skin/24x24/Solid/security.svg?primary=light1"
        showIndicator: false
        onClicked: Workflow.openSecuritySettingsScreen()
    }

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Performance")
        icon: "image://skin/24x24/Solid/speed.svg?primary=light1"
        showIndicator: false
        onClicked: Workflow.openPerformanceSettingsScreen()
    }

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Beta Features")
        extraText: qsTr("Early access to new features")
        icon: "image://skin/24x24/Solid/beta_features.svg?primary=light1"
        showIndicator: false
        onClicked: Workflow.openBetaFeaturesScreen()
    }

    LabeledSwitch
    {
        id: notificationsSwitch

        width: parent.width

        icon: "image://skin/24x24/Solid/notifications.svg?primary=light1"
        text: qsTr("Push notifications")
        manualStateChange: true
        interactive: !appContext.pushManager.userUpdateInProgress
        showIndicator: appContext.pushManager.loggedIn && appContext.pushManager.hasOsPermission
        onClicked: appContext.pushManager.setEnabled(checkState == Qt.Unchecked)
        showCustomArea: checkState == Qt.Checked && appContext.pushManager.systemsCount > 1

        Binding
        {
            target: notificationsSwitch
            property: "checkState"
            value: appContext.pushManager.enabledCheckState
        }

        customArea: Text
        {
            id: systemsCountText

            anchors.verticalCenter: parent.verticalCenter

            font.pixelSize: 16
            color: ColorTheme.colors.light16
            text: appContext.pushManager.expertMode
                ? appContext.pushManager.usedSystemsCount
                    + " / " + appContext.pushManager.systemsCount
                : qsTr("All");
        }

        customAreaClickHandler:
            function()
            {
                if (!appContext.pushManager.loggedIn)
                    Workflow.openCloudLoginScreen()
                else if (!appContext.pushManager.hasOsPermission)
                    appContext.pushManager.showOsPushSettings()
                else if (notificationsSwitch.showCustomArea)
                    Workflow.openPushExpertModeScreen()
                else
                    notificationsSwitch.handleSelectorClicked()
            }

        extraText:
        {
            if (!appContext.pushManager.loggedIn)
                return qsTr("Log in to the cloud to use push notifications")

            return appContext.pushManager.hasOsPermission
                ? ""
                : qsTr("Notifications are turned off in the device settings")
        }

        extraTextColor:
        {
            if (!appContext.pushManager.loggedIn)
                return ColorTheme.colors.brand_core

            return appContext.pushManager.hasOsPermission
                ? notificationsSwitch.extraTextStandardColor
                : ColorTheme.colors.red_l2
        }
    }

    LabeledSwitch
    {
        id: appInfoOption

        width: parent.width
        text: qsTr("AppInfo")
        extraText: appContext.appInfo.version()
        icon: "image://skin/24x24/Solid/info.svg?primary=light1"
        showIndicator: false

        onClicked: Workflow.openAppInfoScreen()
    }
}
