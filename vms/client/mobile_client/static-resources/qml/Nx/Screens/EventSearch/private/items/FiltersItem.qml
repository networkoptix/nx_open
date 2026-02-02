// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Mobile.Controls

import nx.vms.client.core

import ".."
import "../selectors"

Item
{
    id: filtersItem

    property string title: qsTr("Search Filters")
    property ScreenController controller: null
    property alias timeSelector: timeSelector
    property alias deviceSelector: deviceSelector

    signal selectorClicked(OptionSelector selector)

    Flickable
    {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: optionSelectors.height

        Column
        {
            id: optionSelectors

            spacing: 4
            width: filtersItem.width


            TimeSelector
            {
                id: timeSelector

                value: controller.searchSetup.timeSelection
                onValueChanged: filtersItem.controller.searchSetup.timeSelection = value
                height: 56

                onClicked: filtersItem.selectorClicked(timeSelector)
            }

            DeviceSelector
            {
                id: deviceSelector

                height: 56
                onValueChanged:
                {
                    filtersItem.controller.searchSetup.cameraSelection = value.selection
                    if (value.selection !== EventSearch.CameraSelection.all)
                        filtersItem.controller.searchSetup.selectedCamerasIds = value.cameras
                }

                value: ({
                    selection: filtersItem.controller.searchSetup.cameraSelection,
                    cameras: filtersItem.controller.searchSetup.selectedCamerasIds})

                onClicked: filtersItem.selectorClicked(deviceSelector)
            }

            SwitchSelector
            {
                height: 56
                visible: windowContext.mainSystemContext.featureAccess.canUseShareBookmark
                    && (controller?.bookmarkSearchSetup ?? false)

                text: qsTr("Shared Only")
                checkState: (controller?.bookmarkSearchSetup?.searchSharedOnly ?? false)
                    ? Qt.Checked
                    : Qt.Unchecked
                onCheckStateChanged:
                {
                    if (controller && controller.bookmarkSearchSetup)
                        controller.bookmarkSearchSetup.searchSharedOnly = checkState === Qt.Checked
                }
            }
        }
    }

    Loader
    {
        active: controller.analyticsSearchMode
            && controller.searchSetup
            && controller.analyticsSearchSetup

        sourceComponent: AnalyticSelectors
        {
            parent: optionSelectors
            searchSetup: controller.searchSetup
            analyticsSearchSetup: controller.analyticsSearchSetup

            onSelectorClicked: (selector) => filtersItem.selectorClicked(selector)
        }
    }
}
