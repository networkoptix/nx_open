// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import nx.vms.client.desktop

import "private"
import "components/private"

StackLayout
{
    required property var viewModel

    property alias apiIntegrations: apiIntegrations
    property alias settingsView: settingsView
    property alias settingsViewHeader: header
    property alias settingsViewPlaceholder: placeholder

    currentIndex:
        Array.from(children).findIndex((widget) => widget.itemType === viewModel.currentItemType)

    Item
    {
        property string itemType: "Placeholder"

        Placeholder
        {
            y: 64
            width: parent.width
            preferredWidth: 260
            headerFont.pixelSize: 14
            imageSource: "image://svg/skin/integrations/integrations_placeholder.svg"
            header: qsTr("Integrations allow the seamless utilization of video analytics on"
                + " various devices from the VMS.\nSelect an Integration to begin configuring its"
                + " parameters.")
        }
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

        onVisibleChanged:
        {
            header.permissions.reset()
            header.authCode.clear()
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
                header.authCode.clear()
                header.forceActiveFocus()
            }
        }
    }
}
