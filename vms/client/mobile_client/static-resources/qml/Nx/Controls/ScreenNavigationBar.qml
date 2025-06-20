// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Mobile
import Nx.Ui

import nx.vms.client.mobile

Rectangle
{
    id: control

    color: ColorTheme.colors.dark4

    readonly property real heightOffset: visible
        ? height
        : 0

    visible: d.currentIndex !== -1
        && windowContext.sessionManager.hasActiveSession
        && d.buttonModel[d.currentIndex].screenId
             === windowContext.depricatedUiController.currentScreen

    x: 0
    y: windowParams.availableHeight - height
    width: parent.width
    height:d.kBarSize

    RowLayout
    {
        y: (control.height - d.buttonSize.height) / 2
        width: parent.width
        height: d.buttonSize.height

        Repeater
        {
            id: repeater

            model: d.buttonModel

            MouseArea
            {
                id: mouseArea

                objectName: modelData.objectName

                readonly property bool isSelected: d.currentIndex === index

                Layout.alignment: Qt.AlignCenter

                visible: modelData.visible ?? true
                width: d.buttonSize.width
                height: d.buttonSize.height

                Rectangle
                {
                    anchors.fill: parent
                    color: isSelected
                        ? ColorTheme.colors.brand_core
                        : "transparent"
                    radius: 6

                    MaterialEffect
                    {
                        anchors.fill: parent
                        clip: true
                        radius: parent.radius
                        mouseArea: mouseArea
                        rippleSize: 160
                        highlightColor: "#30ffffff"
                    }
                }

                ColoredImage
                {
                    anchors.centerIn: parent
                    sourceSize: Qt.size(24, 24)
                    sourcePath: modelData.iconSource
                    primaryColor: isSelected
                        ? ColorTheme.colors.dark1
                        : ColorTheme.colors.light1
                }

                onClicked: d.switchToPage(index)
            }
        }
    }

    Connections
    {
        target: windowContext.depricatedUiController

        function onCurrentScreenChanged()
        {
            for (let index = 0; index !== d.buttonModel.length; ++index)
            {
                if (d.buttonModel[index].screenId
                    === windowContext.depricatedUiController.currentScreen)
                {
                    d.currentIndex = index
                    return;
                }
            }

            d.currentIndex = -1
        }
    }

    QtObject
    {
        id: d

        property int currentIndex: 0
        readonly property real kBarSize: 70
        readonly property size buttonSize: Qt.size(64, 46)
        readonly property var buttonModel: [
            {
                "objectName": "switchToResourcesScreenButton",
                "iconSource": "image://skin/24x24/Outline/camera.svg",
                "screenId": Controller.ResourcesScreen,
                "openScreen": () => Workflow.openResourcesScreen(
                    windowContext.sessionManager.systemName)
            },
            // Icon paths below will be fixed as we get them in core icons pack.
            {
                "objectName": "switchToEventSearchMenuScreenButton",
                "iconSource": "image://skin/navigation/event_search_button.svg",
                "screenId": Controller.EventSearchMenuScreen,
                "visible": windowContext.mainSystemContext?.hasSearchObjectsPermission
                    || windowContext.mainSystemContext?.hasViewBookmarksPermission,
                "openScreen": () => Workflow.openEventSearchMenuScreen()
            },
            {
                "objectName": "switchToMenuScreenButton",
                "iconSource": "image://skin/navigation/menu_button.svg",
                "screenId": Controller.MenuScreen,
                "openScreen": () => Workflow.openMenuScreen()
            }
        ]

        function switchToPage(index)
        {
            if (index < 0)
                return

            Workflow.popCurrentScreen()
            d.buttonModel[index].openScreen()
        }
    }
}
