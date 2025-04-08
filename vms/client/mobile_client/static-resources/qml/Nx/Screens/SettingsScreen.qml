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
                checkState: appContext.settings.liveVideoPreviews
                    ? Qt.Checked
                    : Qt.Unchecked
                icon: lp("/images/live_previews_settings_option.svg")
                extraText: qsTr("Show previews in the cameras list")
                onCheckStateChanged:
                    appContext.settings.liveVideoPreviews = checkState != Qt.Unchecked
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Save passwords")
                checkState: appContext.settings.savePasswords ? Qt.Checked : Qt.Unchecked
                icon: lp("/images/save_passwords_settings_option.svg")
                extraText: qsTr("Automatically log in to servers")
                onClicked:
                {
                    appContext.settings.savePasswords = checkState
                    if (appContext.settings.savePasswords)
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
                                appContext.credentialsHelper.clearSavedPasswords()
                        })
                }
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Security")
                icon: lp("/images/security_settings_option.svg")

                showCustomArea: checkState === Qt.Checked
                checkState: appContext.settings.certificateValidationLevel == Certificate.ValidationLevel.disabled
                    ? Qt.Unchecked
                    : Qt.Checked

                customArea: Text
                {
                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: 16
                    color: ColorTheme.colors.light16
                    text: appContext.settings.certificateValidationLevel
                        == Certificate.ValidationLevel.recommended
                        ? qsTr("Recommended")
                        : qsTr("Strict")
                }

                onClicked:
                {
                    if (checkState == Qt.Checked)
                    {
                        appContext.settings.certificateValidationLevel =
                            Certificate.ValidationLevel.recommended
                    }
                    else
                    {
                        appContext.settings.certificateValidationLevel =
                            Certificate.ValidationLevel.disabled
                    }
                }

                customAreaClickHandler: ()=>Workflow.openSecuritySettingsScreen()
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Use server time")
                checkState: appContext.settings.serverTimeMode ? Qt.Checked : Qt.Unchecked
                icon: lp("/images/server_time_settings_option.svg")
                onCheckStateChanged:
                {
                    appContext.settings.serverTimeMode = checkState != Qt.Unchecked
                }
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Enable hardware acceleration")
                extraText: qsTr("Increase performance and battery life")
                icon: lp("/images/hardware_decoder.svg")
                checkState: appContext.settings.enableHardwareDecoding
                    ? Qt.Checked
                    : Qt.Unchecked
                onCheckStateChanged:
                    appContext.settings.enableHardwareDecoding = checkState != Qt.Unchecked
            }

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Enable software decoder fallback")
                extraText: qsTr("Decode some rare video formats using software decoder")
                icon: lp("/images/decoder_fallback.svg")
                checkState: appContext.settings.enableSoftwareDecoderFallback
                    ? Qt.Checked
                    : Qt.Unchecked
                onCheckStateChanged: appContext.settings.enableSoftwareDecoderFallback =
                    checkState != Qt.Unchecked
            }

            LabeledSwitch
            {
                id: notificationsSwitch

                width: parent.width

                icon: lp("/images/notifications_settings_option.svg")
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
