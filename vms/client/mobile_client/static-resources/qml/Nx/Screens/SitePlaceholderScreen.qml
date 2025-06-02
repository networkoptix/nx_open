// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls
import Nx.Items
import Nx.Ui

Page
{
    id: sitePlaceholderScreen
    objectName: "sitePlaceholderScreen"

    padding: 0

    onLeftButtonClicked: Workflow.popCurrentScreen()

    Placeholder
    {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2

        text: qsTr("Access to Resources Denied")
        description: qsTr("Sites in the Suspended or Shutdown state are not available")
        imageSource: "image://skin/64x64/Outline/no_access.svg?primary=light10"
    }
}
