// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Ui

import nx.vms.client.mobile

Page
{
    id: page

    property bool activePage: StackView.status === StackView.Active
    property int pageStatus: StackView.status
    property bool sideNavigationEnabled: true
    property alias backgroundColor: backgroundRectangle.color
    property var customBackHandler
    property string result

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
