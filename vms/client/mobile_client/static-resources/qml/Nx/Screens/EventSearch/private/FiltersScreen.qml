// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Mobile.Controls 1.0
import Nx.Ui 1.0

import nx.vms.client.core 1.0

import "."
import "selectors"

Page
{
    id: screen

    title: qsTr("Search Filters")
    objectName: "eventSearchFiltersScreen"

    property ScreenController controller: null

    customBackHandler: () => d.applyChanges()
    onLeftButtonClicked: d.applyChanges()

    Flickable
    {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: optionSelectors.height

        Column
        {
            id: optionSelectors

            spacing: 4
            width: parent.width


            TimeSelector
            {
                value: controller.searchSetup.timeSelection
                onValueChanged: screen.controller.searchSetup.timeSelection = value
            }

            DeviceSelector
            {
                onValueChanged:
                {
                    screen.controller.searchSetup.cameraSelection = value.selection
                    if (value.selection !== EventSearch.CameraSelection.all)
                        screen.controller.searchSetup.selectedCamerasIds = value.cameras
                }

                value: ({
                    selection: screen.controller.searchSetup.cameraSelection,
                    cameras: screen.controller.searchSetup.selectedCamerasIds})
            }
        }
    }

    Loader
    {
        active: controller.analyticsSearchMode

        sourceComponent: AnalyticSelectors
        {
            parent: optionSelectors
            searchSetup: controller && controller.searchSetup
            analyticsSearchSetup: controller && controller.analyticsSearchSetup
        }
    }

    NxObject
    {
        id: d

        property bool hasChanges: false

        Connections
        {
            target: controller.searchSetup
            function onParametersChanged()
            {
                d.hasChanges = true
            }
        }

        Connections
        {
            target: controller.analyticsSearchSetup
            function onParametersChanged()
            {
                d.hasChanges = true
            }
        }

        function applyChanges()
        {
            if (screen.controller && d.hasChanges)
                screen.controller.requestUpdate()

            Workflow.popCurrentScreen()
        }

        Component.onCompleted:
        {
            d.hasChanges = false
        }
    }
}
