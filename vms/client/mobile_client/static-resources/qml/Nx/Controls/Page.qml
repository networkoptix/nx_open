// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as QuickControls
import Nx.Controls
import Nx.Core
import Nx.Ui

import nx.vms.client.mobile

QuickControls.Page
{
    id: control

    property bool activePage: StackView.status === StackView.Active
    property int pageStatus: StackView.status
    property bool sideNavigationEnabled: true
    property var customBackHandler
    property string result

    property alias leftButtonIcon: toolBar.leftButtonIcon
    property alias titleControls: toolBar.controls
    property alias warningText: warningPanel.text
    property alias warningVisible: warningPanel.opened
    property alias toolBar: toolBar
    property alias warningPanel: warningPanel
    property alias titleLabelOpacity: toolBar.titleOpacity
    property alias backgroundColor: backgroundRectangle.color
    property alias gradientToolbarBackground: toolBar.useGradientBackground

    signal leftButtonClicked()
    signal headerClicked()

    clip: true

    header: Item
    {
        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        Column
        {
            id: column
            width: parent.width
            topPadding: toolBar.statusBarHeight

            ToolBar
            {
                id: toolBar
                title: control && control.title
                leftButtonIcon.source: lp("/images/arrow_back.png")
                onLeftButtonClicked: control.leftButtonClicked()
                onClicked: control.headerClicked()
            }

            WarningPanel
            {
                id: warningPanel
            }
        }
    }

    background: Rectangle
    {
        id: backgroundRectangle

        color: ColorTheme.colors.windowBackground
    }

    onSideNavigationEnabledChanged: updateSideNavigation()
    onActivePageChanged:
    {
        if (activePage)
            updateSideNavigation()
    }

    Keys.onPressed: (event) =>
    {
        if (CoreUtils.keyIsBack(event.key))
        {
            if (page.customBackHandler)
            {
                page.customBackHandler()
            }
            else
            {
                if (sideNavigation.opened)
                {
                    sideNavigation.close()
                }
                else if (sessionManager.hasConnectingSession
                    || sessionManager.hasAwaitingResourcesSession)
                {
                    sessionManager.stopSession();
                }
                else if (stackView.depth > 1)
                {
                    Workflow.popCurrentScreen()
                }
                else if (event.key != Qt.Key_Escape)
                {
                    quitApplication()
                }
                else
                {
                    return
                }
            }

            event.accepted = true
        }
        else if (event.key == Qt.Key_F2)
        {
            if (sideNavigationEnabled)
                sideNavigation.open()

            event.accepted = true
        }
    }

    function updateSideNavigation()
    {
        if (!sideNavigationEnabled)
            sideNavigation.close()

        sideNavigation.enabled = sideNavigationEnabled
    }
}
