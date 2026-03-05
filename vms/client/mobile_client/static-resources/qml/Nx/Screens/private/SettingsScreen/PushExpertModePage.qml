// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

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

    opacity: enabled ? 1.0 : 0.3
    enabled: !appContext.pushManager.userUpdateInProgress

    title: !simpleModeRadioButton.checked && d.selectionModel
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
        enabled: !appContext.pushManager.userUpdateInProgress

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

                        onClicked:
                        {
                            if (!appContext.pushManager.loggedIn)
                                Workflow.openCloudLoginScreen()
                            else if (!appContext.pushManager.hasOsPermission)
                                appContext.pushManager.showOsPushSettings()
                            else
                                appContext.pushManager.setEnabled(checkState == Qt.Unchecked)
                        }

                        Binding on checkState { value: appContext.pushManager.enabledCheckState }

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
                        width: parent.width
                        text: qsTr("All Sites")
                    }

                    StyledRadioButton
                    {
                        id: expertModeRadioButton

                        visible: d.loggedInAndHasPermissions
                            && notificationsSwitch.checkState === Qt.Checked
                        width: parent.width
                        text: qsTr("Selected Sites")
                        backgroundRadius: 8
                    }
                }
            }

            Text
            {
                height: 46
                width: parent.width - leftPadding - rightPadding
                leftPadding: 16
                rightPadding: leftPadding

                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 14
                color: ColorTheme.colors.light12
                text: qsTr("SELECT")
                opacity: enabled ? 1.0 : 0.3

                visible: expertModeRadioButton.checked
            }
        }

        ListView
        {
            clip: true
            spacing: 4
            anchors.top: topControlsHolder.bottom
            anchors.bottom: parent.bottom
            width: parent.width
            visible: expertModeRadioButton.checked
            model: d.selectionModel

            delegate: StyledCheckBox
            {
                id: checkbox

                rightPadding: 18
                width: parent ? parent.width : 0
                text: model.systemName
                iconSource: lp("images/cloud_item_marker.svg")

                onCheckStateChanged:
                {
                    if (d.selectionModel)
                        d.selectionModel.setCheckedState(index, checkState)
                }

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
        blurSource: pushExpertModePage
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

            return expertModeRadioButton.checked
                && (d.selectionModel ? d.selectionModel.hasChanges : false)
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
            if (!d.selectionModel)
                return

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

                    if (d.selectionModel)
                    {
                        if (!expertMode)
                            d.selectionModel.selectedSystems = []

                        d.selectionModel.resetChangesFlag()
                    }

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
