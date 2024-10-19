// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.15 as QuickControls

import Nx.Controls 1.0
import Nx.Core 1.0
import Nx.Dialogs 1.0
import Nx.Settings 1.0
import Nx.Ui 1.0

import nx.vms.client.core 1.0
import nx.vms.client.mobile 1.0

Page
{
    id: settingsScreen
    objectName: "developerSettingsScreen"

    title: qsTr("Developer Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    padding: 16

    DeveloperSettingsHelper
    {
        id: helper
    }

    Column
    {
        spacing: 16

        Button
        {
            text: "Change Log Level: " + helper.logLevel
            onClicked:
            {
                logLevelDialog.open()
            }
        }

        Row
        {
            visible: !d.logSessionId
            spacing: 8
            Button
            {
                text: "Upload logs next"
                onClicked:
                {
                    d.logSessionId = LogManager.startRemoteLogSession(
                        parseInt(minutesComboBox.currentValue))
                }
            }

            QuickControls.ComboBox
            {
                id: minutesComboBox

                anchors.verticalCenter: parent.verticalCenter
                model: [1, 3, 5, 10, 15, 30]
                width: 80
            }

            Text
            {
                anchors.verticalCenter: parent.verticalCenter
                text: "minute(s)"
                font.pixelSize: 16
                color: ColorTheme.colors.light16
            }
        }

        Row
        {
            spacing: 8

            Text
            {
                text: "Language"
                font.pixelSize: 16
                color: ColorTheme.colors.light16
                anchors.verticalCenter: parent.verticalCenter
            }

            QuickControls.ComboBox
            {
                id: languageComboBox

                model: TranslationListModel {}
                currentIndex: !!settings.locale
                    ? model.localeIndex(settings.locale)
                    : -1
                textRole: "display"
                valueRole: "localeCode"
                width: 200

                onCurrentValueChanged: settings.locale = currentValue
            }
        }

        Row
        {
            spacing: 8

            TextField
            {
                id: customCloudHostTextField

                text: settings.customCloudHost
                width: settingsScreen.width / 2
            }

            Button
            {
                text: "Set cloud host"

                onClicked:
                {
                    const value = customCloudHostTextField.text.trim()
                    if (value === settings.customCloudHost)
                        return

                    settings.customCloudHost = value
                    d.openRestartDialog()
                }
            }
        }

        LabeledSwitch
        {
            id: forceTrafficLoggingSwitch

            width: parent.width
            text: "Force traffic logging"
            checkState: settings.forceTrafficLogging ? Qt.Checked : Qt.Unchecked
            onCheckStateChanged:
            {
                const value = checkState != Qt.Unchecked
                if (value == settings.forceTrafficLogging)
                    return

                settings.forceTrafficLogging = value
            }
        }

        LabeledSwitch
        {
            id: ignoreCustomizationSwitch

            width: parent.width
            text: "Ignore customization"
            checkState: settings.ignoreCustomization ? Qt.Checked : Qt.Unchecked
            onCheckStateChanged:
            {
                const value = checkState != Qt.Unchecked
                if (value == settings.ignoreCustomization)
                    return

                settings.ignoreCustomization = value
                d.openRestartDialog()
            }
        }

        Column
        {
            visible: !!d.logSessionId

            Text
            {
                text: "Copied to clipboard:"
                font.pixelSize: 16
                color: ColorTheme.colors.light16
            }

            Text
            {
                font.pixelSize: 16
                text: d.logSessionId
                color: ColorTheme.colors.light1
            }

            Button
            {
                text: qsTr("Copy log ID")
                onClicked: d.copySessionIdToClipboard()
            }
        }
    }

    ItemSelectionDialog
    {
        id: logLevelDialog
        title: qsTr("Log Level")
        currentItem: helper.logLevel
        onCurrentItemChanged: helper.logLevel = currentItem
        model: [
            "NONE",
            "ALWAYS",
            "ERROR",
            "WARNING",
            "INFO",
            "DEBUG1",
            "DEBUG2"]
    }

    NxObject
    {
        id: d

        function copySessionIdToClipboard()
        {
            if (logSessionId)
                copyToClipboard(d.logSessionId)
        }

        function openRestartDialog()
        {
            Workflow.openStandardDialog("Please restart the app to apply the changes")
        }

        property string logSessionId: LogManager.remoteLogSessionId()

        onLogSessionIdChanged: copySessionIdToClipboard()
        Component.onCompleted: copySessionIdToClipboard()
    }
}
