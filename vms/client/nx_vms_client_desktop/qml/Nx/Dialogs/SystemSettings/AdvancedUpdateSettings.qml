// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQml 2.14
import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Dialog
{
    title: qsTr("Advanced Update Settings")
    width: 400
    height: 400
    minimumWidth: 300
    minimumHeight: 300
    modality: Qt.ApplicationModal

    property ClientUpdateManager clientUpdateManager: null
    property alias notifyAboutUpdates: notifyAboutUpdatesCheckbox.checked
    property bool offlineUpdatesEnabled: false
    property bool advancedMode: false

    contentItem: Column
    {
        Panel
        {
            width: parent.width

            title: qsTr("Notifications")

            CheckBox
            {
                id: notifyAboutUpdatesCheckbox
                text: qsTr("Notify about available updates")
            }
        }

        Panel
        {
            id: autoClientUpdatesPanel

            width: parent.width
            title: qsTr("Automatic client updates")
            checkable: true

            Binding
            {
                target: autoClientUpdatesPanel
                property: "checked"
                value: clientUpdateManager && clientUpdateManager.clientUpdateEnabled
            }

            property var updateState:
            {
                if (!clientUpdateManager)
                    return {"enabled": false}

                return {
                    "enabled": clientUpdateManager.clientUpdateEnabled,
                    "installationNeeded": clientUpdateManager.installationNeeded,
                    "date": clientUpdateManager.plannedUpdateDate,
                    "version": clientUpdateManager.pendingVersion,
                    "errorMessage": clientUpdateManager.errorMessage
                }
            }

            function dateFormat()
            {
                const format = Qt.locale().dateFormat(Locale.ShortFormat)
                const dayPos = format.indexOf("d")
                const monthPos = format.indexOf("M")
                return dayPos < monthPos ? "dd MMMM" : "MMMM dd"
            }

            property var uiState:
            {
                if (!clientUpdateManager)
                    return {"text": ""}

                if (!updateState.enabled)
                {
                    return {
                        "text": qsTr("Turn this option on to make connected clients update"
                            + " automatically to the latest compatible version.")
                    }
                }

                const noNewVersionState =
                    {
                        "text": qsTr("Connecting clients will be automatically updated to the"
                            + " new version when it’s available."),
                        "checkUpdatesButton": true
                    }

                if (updateState.version.isNull())
                {
                    if (!updateState.errorMessage)
                        return noNewVersionState

                    return {
                        "text": "",
                        "errorMessage": updateState.errorMessage,
                        "checkUpdatesButton": true
                    }
                }

                // TODO: Figure out why SoftwareVersion.toString() does not accept Format from QML.
                const version = updateState.version.major
                    + "." + updateState.version.minor
                    + "." + updateState.version.bugfix
                const releaseNotesUrl = clientUpdateManager.releaseNotesUrl()
                const versionString = releaseNotesUrl
                    ? "<a href=%2>%1</a>".arg(version).arg(releaseNotesUrl)
                    : version

                if (clientUpdateManager.isPlannedUpdateDatePassed())
                {
                    if (!updateState.installationNeeded)
                        return noNewVersionState

                    return {
                        "text": qsTr("All connecting clients are updating to version %1.").arg(
                            versionString),
                        "errorMessage": updateState.errorMessage
                    }
                }

                return {
                    "text": qsTr("Connecting clients will be updated to version %1 on %2.")
                        .arg(versionString)
                        .arg(Qt.formatDate(updateState.date, dateFormat())),
                    "errorMessage": updateState.errorMessage,
                    "speedUpButton": true
                }
            }

            contentItem: Column
            {
                spacing: 8
                width: parent.width

                Label
                {
                    width: parent.width
                    text: autoClientUpdatesPanel.uiState.text
                    wrapMode: Text.WordWrap
                    textFormat: Text.RichText
                    visible: !!text

                    onLinkActivated: link => Qt.openUrlExternally(link)
                }

                Label
                {
                    width: parent.width
                    text: autoClientUpdatesPanel.uiState.errorMessage ?? ""
                    wrapMode: Text.WordWrap
                    color: ColorTheme.colors.red_core
                    visible: !!text
                }

                TextButton
                {
                    text: qsTr("Check for updates")
                    icon.source: "image://svg/skin/text_buttons/reload_20.svg"
                    visible: !!autoClientUpdatesPanel.uiState.checkUpdatesButton

                    onClicked:
                    {
                        if (clientUpdateManager)
                            clientUpdateManager.checkForUpdates()
                    }
                }

                TextButton
                {
                    text: qsTr("Speed up this update")
                    icon.source: "image://svg/skin/text_buttons/shopping_cart.svg"
                    visible: advancedMode && !!autoClientUpdatesPanel.uiState.speedUpButton

                    onClicked:
                    {
                        if (clientUpdateManager)
                            clientUpdateManager.speedUpCurrentUpdate()
                    }
                }
            }

            onTriggered: checked =>
            {
                if (clientUpdateManager)
                    clientUpdateManager.clientUpdateEnabled = checked
            }
        }

        Panel
        {
            id: offlineUpdatesPanel

            width: parent.width
            title: qsTr("Offline client updates")
            checkable: true

            Binding on checked { value: offlineUpdatesEnabled }

            contentItem: Label
            {
                width: parent.width
                wrapMode: Text.WordWrap

                text: offlineUpdatesPanel.checked
                    ? qsTr("Offline updates are enabled and files are stored on servers.")
                    : qsTr("Turning this option on will enable downloading of additional files to "
                        + "servers which can be used to update connecting clients even without "
                        + "internet access.")
            }

            onTriggered: checked =>
            {
                if (!checked)
                {
                    const result = MessageBox.exec(
                        MessageBox.Icon.Question,
                        qsTr("This will delete all update files on servers.") + "\n"
                            + qsTr("Proceed anyway?"),
                        "",
                        MessageBox.Cancel,
                        {"text": qsTr("Yes"), "role": DialogButtonBox.YesRole}
                    )

                    if (result === MessageBox.Cancel)
                    {
                        offlineUpdatesPanel.checked = true
                        return
                    }
                }

                offlineUpdatesEnabled = checked
            }
        }
    }

    DialogBanner
    {
        height: visible ? implicitHeight : 0
        visible: Branding.isDesktopClientCustomized()
        anchors.bottom: buttonBox.top
        width: parent.width
        text: qsTr("You are using a custom client. Please contact %1 to get the update "
            + "instructions.").arg(Branding.company())
    }

    buttonBox: DialogButtonBox
    {
        standardButtons: DialogButtonBox.Ok
    }
}
