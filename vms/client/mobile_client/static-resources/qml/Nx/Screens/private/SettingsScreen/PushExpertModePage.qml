// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core
import Nx.Controls
import Nx.Mobile
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
        enabled: !appContext.pushManager.userUpdateInProgress

        width: parent.width
        height: parent.height

        Column
        {
            id: topControlsHolder

            width: parent.width

            Rectangle
            {
                width: parent.width
                height: notificationSourceGroup.implicitHeight
                radius: LayoutController.isTablet ? 8 : 0
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

                        showIndicator:
                            appContext.pushManager.loggedIn && appContext.pushManager.hasOsPermission

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

                            x: parent.leftPadding
                            width: parent.width - x - parent.rightPadding
                            anchors.bottom: parent.bottom

                            height: 1
                            color: ColorTheme.colors.dark8
                        }
                    }

                    StyledRadioButton
                    {
                        id: simpleModeRadioButton

                        width: parent.width
                        text: qsTr("All Sites")
                    }

                    StyledRadioButton
                    {
                        id: expertModeRadioButton

                        width: parent.width
                        text: qsTr("Selected Sites")
                        backgroundRadius: LayoutController.isTablet ? 8 : 0
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
            windowContext.ui.showMessage(title, message)
        }
    }

    NxObject
    {
        id: d

        property bool hasChanges:
        {
            if (appContext.pushManager.expertMode != expertModeRadioButton.checked)
                return true

            return expertModeRadioButton.checked
                && (d.selectionModel ? d.selectionModel.hasChanges : false)
        }

        property PushSystemsSelectionModel selectionModel: PushSystemsSelectionModel {}

        function tryApplyAndReturn(successCallback)
        {
            if (!d.selectionModel)
                return

            const expertMode = expertModeRadioButton.checked
            if (expertMode && !d.selectionModel.selectedSystems.length)
            {
                Workflow.openStandardDialog(
                    qsTr("At least one site has to be selected"))
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

            var saveButton = {id: "saveChangesButton", text: qsTr("Yes")}
            var goBackButton = {id: "goBackButton", text: qsTr("No")}
            var buttons = [goBackButton, saveButton];

            var dialog = Workflow.openStandardDialog("", qsTr("Save changes?"), buttons, true)
            dialog.buttonClicked.connect(
                function(buttonId)
                {
                    switch(buttonId)
                    {
                        case "saveChangesButton":
                            d.tryApplyAndReturn()
                            break
                        case "goBackButton":
                            reset()
                            pushExpertModePage.settingsSaved()
                    }
                });
        }

        function reset()
        {
            expertModeRadioButton.checked = appContext.pushManager.expertMode
            simpleModeRadioButton.checked = !appContext.pushManager.expertMode
            d.selectionModel.reset()
        }
    }
}
