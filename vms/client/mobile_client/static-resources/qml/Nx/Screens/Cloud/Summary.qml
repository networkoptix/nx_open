// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Ui

import "private"

Page
{
    id: cloudScreen
    objectName: "cloudSummaryScreen"

    padding: 8
    topPadding: 0

    onLeftButtonClicked: Workflow.popCurrentScreen()

    title: qsTr("%1 Account", "%1 is the short cloud name (like 'Cloud')")
        .arg(applicationInfo.shortCloudName())

    Column
    {
        id: column

        width: parent.width
        topPadding: 24

        CloudBanner
        {
            id: cloudBanner
            text: cloudStatusWatcher.cloudLogin
        }

        Item { width: 1; height: 18 }

        Button
        {
            id: logoutButton
            text: qsTr("Log out")
            width: parent.width

            onClicked:
            {
                cloudStatusWatcher.resetAuthData();
                Workflow.popCurrentScreen()
            }
        }

        Item { width: 1; height: 10 }

        LinkButton
        {
            text: qsTr("Go to %1", "%1 is the short cloud name (like 'Cloud')").arg(
                applicationInfo.cloudName())
            width: parent.width
            url: cloudUrlHelper.mainUrl()
        }
    }
}
