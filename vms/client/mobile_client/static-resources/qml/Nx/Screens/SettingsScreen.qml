// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Settings 1.0
import Nx.Ui 1.0

import nx.vms.client.core 1.0

Page
{
    id: settingsScreen

    objectName: "settingsScreen"

    title: qsTr("Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    topPadding: 4
    bottomPadding: 16

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: width
        contentHeight: content.height

        Column
        {
            id: content

            width: flickable.width
            spacing: 4

            LabeledSwitch
            {
                id: livePreviews

                width: parent.width
                text: qsTr("Live previews")
                checkState: settings.liveVideoPreviews ? Qt.Checked : Qt.Unchecked
                icon: lp("/images/live_previews_settings_option.svg")
                extraText: qsTr("Show previews in the cameras list")
                onCheckStateChanged: settings.liveVideoPreviews = checkState != Qt.Unchecked
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Save passwords")
                checkState: settings.savePasswords ? Qt.Checked : Qt.Unchecked
                icon: lp("/images/save_passwords_settings_option.svg")
                extraText: qsTr("Automatically log in to servers")
                onClicked:
                {
                    settings.savePasswords = checkState
                    if (settings.savePasswords)
                        return

                    var dialog = Workflow.openStandardDialog(
                        "", qsTr("What to do with currently saved passwords?"),
                        [
                            qsTr("Keep"),
                            {"id": "DELETE", "text": qsTr("Delete"), "accented": true}
                        ], true)

                    dialog.buttonClicked.connect(
                        function(buttonId)
                        {
                            if (buttonId == "DELETE")
                                clearSavedPasswords()
                        })
                }
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Security")
                icon: lp("/images/security_settings_option.svg")

                showCustomArea: checkState === Qt.Checked
                checkState: settings.certificateValidationLevel == Certificate.ValidationLevel.disabled
                    ? Qt.Unchecked
                    : Qt.Checked

                customArea: Text
                {
                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: 16
                    color: ColorTheme.colors.light16
                    text: settings.certificateValidationLevel
                          == Certificate.ValidationLevel.recommended
                          ? qsTr("Recommended")
                          : qsTr("Strict")
                }

                onClicked:
                {
                    if (checkState == Qt.Checked)
                        settings.certificateValidationLevel = Certificate.ValidationLevel.recommended
                    else
                        settings.certificateValidationLevel = Certificate.ValidationLevel.disabled
                }

                customAreaClickHandler: ()=>Workflow.openSecuritySettingsScreen()
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Use server time")
                checkState: serverTimeMode ? Qt.Checked : Qt.Unchecked
                icon: lp("/images/server_time_settings_option.svg")
                onCheckStateChanged:
                {
                    serverTimeMode = checkState != Qt.Unchecked
                }
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Enable hardware acceleration")
                extraText: qsTr("Increase performance and battery life")
                icon: lp("/images/hardware_decoder.svg")
                checkState: settings.enableHardwareDecoding  ? Qt.Checked : Qt.Unchecked
                onCheckStateChanged: settings.enableHardwareDecoding = checkState != Qt.Unchecked
            }

            LabeledSwitch
            {
                id: notificationsSwitch

                width: parent.width

                icon: lp("/images/notifications_settings_option.svg")
                text: qsTr("Push notifications")
                manualStateChange: true
                interactive: !pushManager.userUpdateInProgress
                showIndicator: pushManager.loggedIn && pushManager.hasOsPermission
                onClicked: pushManager.setEnabled(checkState == Qt.Unchecked)
                showCustomArea: checkState == Qt.Checked && pushManager.systemsCount > 1

                Binding
                {
                    target: notificationsSwitch
                    property: "checkState"
                    value: pushManager.enabledCheckState
                }

                customArea: Text
                {
                    id: systemsCountText

                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: 16
                    color: ColorTheme.colors.light16
                    text: pushManager.expertMode
                        ? pushManager.usedSystemsCount + " / " + pushManager.systemsCount
                        : qsTr("All");
                }

                customAreaClickHandler:
                    function()
                    {
                        if (!pushManager.loggedIn)
                            Workflow.openCloudLoginScreen()
                        else if (!pushManager.hasOsPermission)
                            pushManager.showOsPushSettings()
                        else if (notificationsSwitch.showCustomArea)
                            Workflow.openPushExpertModeScreen()
                        else
                            notificationsSwitch.handleSelectorClicked()
                    }

                extraText:
                {
                    if (!pushManager.loggedIn)
                        return qsTr("Log in to the cloud to use push notifications")

                    return pushManager.hasOsPermission
                        ? ""
                        : qsTr("Push notifications are turned off in site settings")
                }

                extraTextColor:
                {
                    if (!pushManager.loggedIn)
                        return ColorTheme.colors.brand_core

                    return pushManager.hasOsPermission
                        ? notificationsSwitch.extraTextStandardColor
                        : ColorTheme.colors.red_l2
                }
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Beta Features")
                extraText: qsTr("Early access to new features")
                icon: lp("/images/beta_features.svg")
                showIndicator: false
                onClicked: Workflow.openBetaFeaturesScreen()
            }
        }
    }
}
