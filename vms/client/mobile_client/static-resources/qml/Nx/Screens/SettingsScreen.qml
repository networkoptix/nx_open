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
        icon: "image://skin/24x24/Solid/notifications.svg?primary=light1"
        text: qsTr("Notifications")
        showIndicator: false
        onClicked: Workflow.openPushExpertModeScreen()
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
        icon: "image://skin/24x24/Solid/beta_features.svg?primary=light1"
        showIndicator: false
        onClicked: Workflow.openBetaFeaturesScreen()
    }

    LabeledSwitch
    {
        id: appInfoOption

        width: parent.width
        text: qsTr("About")
        icon: "image://skin/24x24/Solid/info.svg?primary=light1"
        showIndicator: false

        onClicked: Workflow.openAppInfoScreen()
    }
}
