// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

Dialog
{
    id: dialog
    objectName: "IntegrationsDialog"

    modality: Qt.ApplicationModal

    width: minimumWidth
    height: minimumHeight

    minimumWidth: 768
    minimumHeight: 700

    title: qsTr("Manage Integrations")
    color: ColorTheme.colors.dark7

    property var store: null
    property var requestsModel: store ? store.makeApiIntegrationRequestsModel() : null

    WindowContextAware.onBeforeSystemChanged: reject()

    DialogTabControl
    {
        anchors.fill: parent
        anchors.bottomMargin: buttonBox.height

        dialogLeftPadding: dialog.leftPadding
        dialogRightPadding: dialog.rightPadding
        tabBar.spacing: 0

        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("Integrations")
            }

            page: IntegrationsTab
            {
                store: dialog.store
                requestsModel: dialog.requestsModel
                visible: dialog.visible
            }
        }

        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("Settings")
            }

            page: SettingsTab
            {
                requestsModel: dialog.requestsModel
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Apply | DialogButtonBox.Cancel

        Component.onCompleted:
        {
            let applyButton = buttonBox.standardButton(DialogButtonBox.Apply)
            applyButton.enabled = Qt.binding(function() { return !!store && store.hasChanges })
        }
    }

    onAccepted:
    {
        if (store)
            store.applySettingsValues()
    }

    onApplied:
    {
        if (store)
            store.applySettingsValues()
    }

    onRejected:
    {
        if (store)
            store.discardChanges()
    }
}
