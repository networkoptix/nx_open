// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Mobile
import Nx.Mobile.Popups
import Nx.Settings
import Nx.Ui

BaseSettingsPage
{
    id: pushExpertModePage

    objectName: "pushExpertModePage"

    function saveSettings()
    {
        d.handleBackClicked()
    }

    title: !simpleModeRadioButton.checked
        ? "%1: %2".arg(qsTr("Sites")).arg(d.selectionModel.selectedSystems.length)
        : qsTr("Notifications")

    rightControl: Button
    {
        id: doneButton

        visible: d.hasChanges
        enabled: !appContext.pushManager.userUpdateInProgress

        text: qsTr("Done")
        color: "transparent"
        hoveredColor: color
        highlightColor: color
        textColor: ColorTheme.colors.brand_core
        pressedTextColor: ColorTheme.colors.brand_d1
        padding: 0
        leftPadding: 0
        rightPadding: 0

        onClicked: d.tryApplyAndReturn()
    }

    Item
    {
        id: content

        width: parent.width
        height: pushExpertModePage.availableHeight

        Column
        {
            id: topControlsHolder

            width: parent.width

            Rectangle
            {
                width: parent.width
                height: notificationSourceGroup.implicitHeight
                radius: 8
                color: ColorTheme.colors.dark6

                Column
                {
                    id: notificationSourceGroup

                    spacing: 4
                    width: parent.width

                    LabeledSwitch
                    {
                        id: notificationsSwitch

                        width: parent.width

                        text: qsTr("Notifications")
                        manualStateChange: true
                        showIndicator: d.loggedInAndHasPermissions
                        checkState: appContext.pushManager.enabledCheckState

                        onClicked:
                        {
                            if (!appContext.pushManager.loggedIn)
                            {
                                Workflow.openCloudLoginScreen()
                            }
                            else if (!appContext.pushManager.hasOsPermission)
                            {
                                appContext.pushManager.showOsPushSettings()
                            }
                            else
                            {
                                const notificationsEnabled = checkState == Qt.Checked

                                appContext.pushManager.setEnabled(!notificationsEnabled)
                                if (notificationsEnabled)
                                    d.reset()
                            }
                        }

                        extraText:
                        {
                            if (!appContext.pushManager.loggedIn)
                                return qsTr("Log in to the cloud to receive notifications")

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

                        Rectangle
                        {
                            id: line

                            anchors.bottom: parent.bottom
                            x: parent.leftPadding
                            width: parent.width - x - parent.rightPadding
                            height: 1
                            visible: notificationsSwitch.showIndicator
                                && notificationsSwitch.checkState === Qt.Checked
                            color: ColorTheme.colors.dark8
                        }
                    }

                    StyledRadioButton
                    {
                        id: simpleModeRadioButton

                        visible: d.loggedInAndHasPermissions
                            && notificationsSwitch.checkState === Qt.Checked

                        enabled: !appContext.pushManager.userUpdateInProgress
                        width: parent.width
                        text: qsTr("All Sites")
                        font.pixelSize: 18
                    }

                    StyledRadioButton
                    {
                        id: expertModeRadioButton

                        visible: d.loggedInAndHasPermissions
                            && notificationsSwitch.checkState === Qt.Checked
                        width: parent.width

                        enabled: !appContext.pushManager.userUpdateInProgress
                        text: qsTr("Selected Sites")
                        backgroundRadius: 8
                        font.pixelSize: 18
                    }
                }
            }

            Text
            {
                width: parent.width - leftPadding - rightPadding
                topPadding: 20
                bottomPadding: 12
                leftPadding: 16
                rightPadding: leftPadding

                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 14
                font.weight: Font.Medium
                color: ColorTheme.colors.light16
                text: qsTr("SELECTED %1/%2")
                    .arg(d.selectionModel.selectedSystems.length)
                    .arg(systemsList.count)
                enabled: !appContext.pushManager.userUpdateInProgress
                opacity: enabled ? 1.0 : 0.3

                visible: expertModeRadioButton.visible && expertModeRadioButton.checked
            }
        }

        ListView
        {
            id: systemsList

            clip: true
            spacing: 4
            anchors.top: topControlsHolder.bottom
            anchors.bottom: parent.bottom
            width: parent.width
            visible: expertModeRadioButton.visible && expertModeRadioButton.checked
            enabled: !appContext.pushManager.userUpdateInProgress
            model: d.selectionModel

            delegate: StyledCheckBox
            {
                id: checkbox

                width: parent ? parent.width : 0
                height: 56
                topPadding: 16
                bottomPadding: 16

                text: NxGlobals.toHtmlEscaped(model.systemName)
                font.pixelSize: 18
                backgroundRadius: 8
                iconSource: "image://skin/24x24/Outline/cloud.svg?primary=%1"
                    .arg(checkbox.checked ? "brand_core" : "light10")

                onCheckStateChanged: d.selectionModel.setCheckedState(index, checkState)

                Binding
                {
                    target: checkbox
                    property: "checkState"
                    value: model.checkState
                }
            }
        }
    }

    Component.onCompleted: d.reset()

    Connections
    {
        target: appContext.pushManager

        function onUserUpdateInProgressChanged()
        {
            if (appContext.pushManager.userUpdateInProgress)
                preloaderOverlay.open()
            else
                preloaderOverlay.close()
        }
    }

    PreloaderOverlay
    {
        id: preloaderOverlay

        parent: pushExpertModePage
    }

    Connections
    {
        target: appContext.pushManager
        function onShowPushSettingsErrorMessage(title, message)
        {
            d.openErrorDialog(title, message)
        }
    }

    PopupBase
    {
        id: saveDialog

        withCloseButton: false
        icon: "image://skin/48x48/Solid/warning.svg?primary=yellow"
        title: qsTr("Save changes?")

        buttonBoxButtons:
        [
            PopupButton
            {
                text: qsTr("No")

                onClicked:
                {
                    d.reset()
                    pushExpertModePage.settingsSaved()

                    saveDialog.close()
                }
            },

            PopupButton
            {
                text: qsTr("Yes")
                accented: true

                onClicked:
                {
                    d.tryApplyAndReturn()

                    saveDialog.close()
                }
            }
        ]
    }

    NxObject
    {
        id: d

        readonly property bool loggedInAndHasPermissions: appContext.pushManager.loggedIn
            && appContext.pushManager.hasOsPermission

        property bool hasChanges:
        {
            if (appContext.pushManager.expertMode != expertModeRadioButton.checked)
                return true

            return expertModeRadioButton.checked && d.selectionModel.hasChanges
        }

        property PushSystemsSelectionModel selectionModel: PushSystemsSelectionModel {}

        function openErrorDialog(title, message = "")
        {
            Workflow.openDialog(
                "qrc:/qml/Nx/Mobile/Popups/StandardPopup.qml",
                {
                    "title": qsTr("At least one site has to be selected"),
                    "messages": [message],
                    "icon": "image://skin/48x48/Solid/warning.svg?primary=yellow",
                    "accentedOkButton": true
                })
        }

        function tryApplyAndReturn(successCallback)
        {
            const expertMode = expertModeRadioButton.checked
            if (expertMode && !d.selectionModel.selectedSystems.length)
            {
                openErrorDialog(qsTr("At least one site has to be selected"))
                return
            }

            const callback =
                function(success)
                {
                    if (!success)
                    {
                        pushExpertModePage.error()
                        return
                    }

                    if (!expertMode)
                        d.selectionModel.selectedSystems = []

                    d.selectionModel.resetChangesFlag()

                    pushExpertModePage.settingsSaved()
                }

            if (expertModeRadioButton.checked)
                appContext.pushManager.setExpertMode(d.selectionModel.selectedSystems, callback)
            else
                appContext.pushManager.setSimpleMode(callback)
        }

        function handleBackClicked()
        {
            if (!d.hasChanges)
            {
                pushExpertModePage.settingsSaved()
                return
            }

            saveDialog.open()
        }

        function reset()
        {
            expertModeRadioButton.checked = appContext.pushManager.expertMode
            simpleModeRadioButton.checked = !appContext.pushManager.expertMode
            d.selectionModel.reset()
        }
    }
}
