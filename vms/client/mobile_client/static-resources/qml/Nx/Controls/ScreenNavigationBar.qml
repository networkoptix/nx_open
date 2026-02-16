// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Ui

Rectangle
{
    id: control

    color: ColorTheme.colors.dark4
    implicitHeight: d.kBarSize
    implicitWidth: d.kBarSize

    QtObject
    {
        id: d

        readonly property real kBarSize: 72
        readonly property size buttonSize: Qt.size(kBarSize, kBarSize)
        readonly property bool hasObjectsOrBookmarkPermissions:
        {
            if (!windowContext.mainSystemContext)
                return false

            return windowContext.mainSystemContext?.hasSearchObjectsPermission
                || windowContext.mainSystemContext?.hasViewBookmarksPermission
        }
    }

    component NavigationButton : MouseArea
    {
        id: mouseArea

        property bool isSelected: screenId === windowContext.deprecatedUiController.currentScreen
        property alias iconSource: icon.sourcePath
        property alias iconIndicatorText: iconIndicator.text
        required property int screenId

        Layout.alignment: Qt.AlignCenter
        Layout.fillWidth: !LayoutController.isTabletLayout
        Layout.preferredWidth: d.buttonSize.width
        Layout.preferredHeight: d.buttonSize.height

        Rectangle
        {
            anchors.fill: parent
            visible: mouseArea.isSelected
            rotation: LayoutController.isTabletLayout ? 90 : 0
            opacity: 0.1

            gradient: Gradient
            {
                GradientStop { position: 0.0; color: "#0C1012" }
                GradientStop { position: 1.0; color: ColorTheme.colors.brand_core }
            }
        }

        Rectangle
        {
            width: LayoutController.isTabletLayout ? 2 : parent.width
            height: LayoutController.isTabletLayout ? parent.height : 2
            anchors.bottom: parent.bottom
            color: ColorTheme.colors.brand_core
            visible: mouseArea.isSelected
        }

        Rectangle
        {
            anchors.fill: parent
            color: "transparent"

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
            id: icon

            anchors.centerIn: parent
            sourceSize: Qt.size(24, 24)
            primaryColor: isSelected ? ColorTheme.colors.brand_core : ColorTheme.colors.light1

            Indicator
            {
                id: iconIndicator

                visible: !!text
            }
        }
    }

    RowLayout
    {
        id: mobileLayout

        anchors.fill: parent
        visible: !LayoutController.isTabletLayout
        spacing: 24

        LayoutItemProxy
        {
            target: switchToResourcesScreenButton
        }

        LayoutItemProxy
        {
            target: switchToEventSearchMenuScreenButton
            visible: d.hasObjectsOrBookmarkPermissions
        }

        LayoutItemProxy
        {
            target: switchToFeedScreenButton
        }

        LayoutItemProxy
        {
            target: switchToMenuScreenButton
        }
    }

    ColumnLayout
    {
        id: tabletLayout

        anchors.fill: parent
        visible: LayoutController.isTabletLayout
        spacing: 0

        LayoutItemProxy
        {
            target: switchToResourcesScreenButton
        }

        LayoutItemProxy
        {
            target: switchToEventSearchMenuScreenButton
            visible: d.hasObjectsOrBookmarkPermissions
        }

        LayoutItemProxy
        {
            target: switchToFeedScreenButton
        }

        NavigationButton
        {
            id: settingsButton

            objectName: "settingsButton"
            iconSource: "image://skin/24x24/Outline/settings.svg"
            screenId: Controller.SettingsScreen
            onClicked: Workflow.openSettingsScreen(/*push*/ false)
        }

        Item
        {
            Layout.fillHeight: true
        }

        NavigationButton
        {
            id: logoutButton

            objectName: "logoutButton"
            iconSource: "image://skin/24x24/Outline/logout.svg"
            screenId: Controller.UnknownScreen
            onClicked: windowContext.sessionManager.stopSessionByUser()
        }
    }

    NavigationButton
    {
        id: switchToResourcesScreenButton

        objectName: "switchToResourcesScreenButton"
        iconSource: "image://skin/24x24/Outline/camera.svg"
        screenId: Controller.ResourcesScreen
        onClicked: Workflow.openResourcesScreen(windowContext.sessionManager.systemName)
    }

    NavigationButton
    {
        id: switchToEventSearchMenuScreenButton

        objectName: "switchToEventSearchMenuScreenButton"
        iconSource: "image://skin/navigation/event_search_button.svg"
        screenId: Controller.EventSearchScreen
        onClicked: Workflow.openEventSearchScreen(/*push*/ false)
    }

    NavigationButton
    {
        id: switchToFeedScreenButton

        objectName: "switchToFeedScreenButton"
        iconSource: feedStateProvider.buttonIconSource
        iconIndicatorText: feedStateProvider.buttonIconIndicatorText
        screenId: Controller.FeedScreen
        onClicked: Workflow.openFeedScreen(/*push*/ false, feedStateProvider)
    }

    NavigationButton
    {
        id: switchToMenuScreenButton

        objectName: "switchToMenuScreenButton"
        iconSource: "image://skin/navigation/menu_button.svg"
        screenId: Controller.MenuScreen
        onClicked: Workflow.openMenuScreen()
    }

    FeedStateProvider { id: feedStateProvider }
}
