// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import Nx.RightPanel

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

import ".."

SearchPanel
{
    id: objectsPanel

    readonly property AnalyticsSearchSetup analyticsSetup:
        model ? model.analyticsSetup : null

    readonly property Analytics.ObjectTypeModel objectTypeModel:
        model ? model.objectTypeModel : null

    readonly property Analytics.AnalyticsFilterModel analyticsFilterModel:
        objectTypeModel ? objectTypeModel.sourceModel : null

    type: { return RightPanelModel.Type.analytics }
    showInformationButton: true

    placeholder
    {
        icon: "image://skin/64x64/Outline/noobjects.svg"
        title: qsTr("No objects")
        description: qsTr("Try changing the filters or configure object detection "
            + "in the camera plugin settings")
    }

    ColumnLayout
    {
        id: extraFilters

        parent: objectsPanel.filtersColumn
        spacing: 2

        Layout.fillHeight: false

        SelectableTextButton
        {
            id: areaSelectionButton

            Layout.maximumWidth: objectsPanel.filtersColumn.width

            selectable: false
            icon.source: "image://skin/20x20/Outline/frame.svg"
            accented: analyticsSetup && analyticsSetup.areaSelectionActive

            desiredState: analyticsSetup
                && (analyticsSetup.isAreaSelected || analyticsSetup.areaSelectionActive)
                    ? SelectableTextButton.Unselected
                    : SelectableTextButton.Deactivated
            text:
            {
                if (state === SelectableTextButton.Deactivated)
                    return qsTr("Select area")

                return accented
                    ? qsTr("Select some area on the video...")
                    : qsTr("In selected area")
            }

            onDeactivated:
            {
                if (analyticsSetup)
                    analyticsSetup.clearAreaSelection()
            }

            onClicked:
            {
                if (analyticsSetup)
                    analyticsSetup.areaSelectionActive = true
            }
        }

        PluginSelector
        {
            id: pluginSelector

            setup: analyticsSetup
            model: analyticsFilterModel
            Layout.maximumWidth: objectsPanel.filtersColumn.width
        }

        ObjectTypeSelector
        {
            id: objectTypeSelector

            setup: analyticsSetup
            model: objectTypeModel
            Layout.maximumWidth: objectsPanel.filtersColumn.width
        }
    }

    onFiltersReset:
    {
        pluginSelector.deactivate()
        objectTypeSelector.deactivate()
        areaSelectionButton.deactivate()
    }
}
