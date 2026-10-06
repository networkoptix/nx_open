// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Ui

Tutorial
{
    name: "panelButton"
    title: qsTr("Floating Panel Button")
    description: qsTr("Reopen a collapsed panel")
    enabled: LayoutController.hasSidePanels

    trigger.name: "panelButton"
    trigger.when: TutorialTrigger.Available

    TutorialStep
    {
        title: qsTr("Floating Panel Button")
        description: qsTr("Tap the floating button to open the panel")
    }
}
