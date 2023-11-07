// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import nx.vms.client.desktop

import "private"

StackLayout
{
    required property var viewModel

    property alias apiIntegrations: apiIntegrations
    property alias settingsView: settingsView
    property alias settingsViewHeader: header
    property alias settingsViewPlaceholder: placeholder

    currentIndex:
        Array.from(children).findIndex((widget) => widget.itemType === viewModel.currentItemType)

    SettingsPlaceholder
    {
        property string itemType: "Placeholder"
        header: qsTr("Integrations allow the seamless utilization of video analytics on various"
            + " devices from the VMS.\nSelect an Integration to begin configuring its parameters.")
    }

    Plugins
    {
        id: plugins

        property string itemType: "Plugins"

        visible: !LocalSettings.iniConfigValue("integrationsManagement")
    }

    ApiIntegrations
    {
        id: apiIntegrations

        property string itemType: "ApiIntegrations"

        visible: !LocalSettings.iniConfigValue("integrationsManagement")
        requestsModel: visible ? viewModel.requestsModel : null
    }

    SettingsView
    {
        id: settingsView

        property string itemType: "Engine"
        property string engineId: ""

        headerItem: IntegrationPanel
        {
            id: header
            engineInfo: viewModel.currentEngineInfo
            licenseSummary: viewModel.currentEngineLicenseSummary
            request: viewModel.currentRequest
            checked:
                checkable && viewModel.enabledEngines.indexOf(viewModel.currentEngineId) !== -1

            onApproveClicked: (authCode) =>
            {
                viewModel.requestsModel.approve(request.requestId, authCode)
            }

            onRemoveClicked: () =>
            {
                if (request)
                    viewModel.requestsModel.reject(request.requestId)
            }
        }

        placeholderItem: SettingsPlaceholder
        {
            id: placeholder
            visible: !viewModel.currentRequestId
        }

        Connections
        {
            target: viewModel

            function onCurrentSectionPathChanged()
            {
                settingsView.selectSection(viewModel.currentSectionPath)
            }

            function onSettingsViewStateChanged(model, initialValues)
            {
                const currentEngineId =
                    viewModel.currentEngineId ? viewModel.currentEngineId.toString() : ""

                const restoreScrollPosition = settingsView.engineId === currentEngineId
                settingsView.engineId = currentEngineId

                settingsView.loadModel(
                    model,
                    initialValues ?? {},
                    viewModel.currentSectionPath,
                    restoreScrollPosition)
            }

            function onSettingsViewValuesChanged(values, isInitial)
            {
                settingsView.setValues(values ?? {}, isInitial)
            }

            function onCurrentSettingsErrorsChanged()
            {
                settingsView.setErrors(viewModel.currentSettingsErrors)
            }

            function onCurrentItemChanged()
            {
                header.forceActiveFocus()
            }
        }
    }
}
