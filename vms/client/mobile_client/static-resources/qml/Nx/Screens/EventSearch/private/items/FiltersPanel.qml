// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Mobile
import Nx.Ui

import nx.vms.client.core.analytics

import ".."

Rectangle
{
    id: control

    property ScreenController controller: null

    implicitHeight: 56

    color: ColorTheme.colors.dark7

    Row
    {
        id: filtersButtonPanel

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter

        IconButton
        {
            anchors.verticalCenter: parent.verticalCenter

            width: 56
            height: width

            icon.width: 24
            icon.height: icon.height
            icon.source: lp("/images/filter.svg")

            onClicked:
                Workflow.openEventFiltersScreen(control.controller)
        }

        Rectangle
        {
            width: 1
            height: 32
            color: ColorTheme.colors.dark11
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    ListView
    {
        id: parametersList

        height: parent.height
        anchors.left: filtersButtonPanel.right
        anchors.right: parent.right

        clip: true
        spacing: 4
        orientation: Qt.Horizontal

        model: ParametersVisualizationModel
        {
            analyticsSearchSetup: control.controller.analyticsSearchSetup
            searchSetup: control.controller.searchSetup
        }

        header: Item { width: 16 }
        footer: Item { width: 4 }

        delegate: Rectangle
        {
            width: content.width + 8 * 2
            height: 32
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            color: ColorTheme.colors.dark13
            radius: 2
            border.color: ColorTheme.colors.dark15

            Row
            {
                id: content

                spacing: 6
                anchors.centerIn: parent

                ColoredImage
                {
                    width: 20
                    height: width
                    sourceSize: Qt.size(width, height)
                    visible: sourcePath != ""
                    primaryColor: text.color
                    sourcePath: model.decoration
                        ? IconManager.iconUrl(model.decoration)
                        :""
                }

                Text
                {
                    id: text
                    font.pixelSize: 16
                    color: ColorTheme.colors.light4
                    text: model.display
                }
            }

            MouseArea
            {
                anchors.fill: parent
                onClicked: Workflow.openEventFiltersScreen(control.controller)
            }
        }
    }

    Text
    {
        anchors.left: filtersButtonPanel.right
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter

        font.pixelSize: 16
        color: ColorTheme.colors.dark13
        visible: !parametersList.count

        text: qsTr("No filters")
    }
}
