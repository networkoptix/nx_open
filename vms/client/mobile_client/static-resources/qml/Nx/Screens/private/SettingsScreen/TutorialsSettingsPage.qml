// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

BaseSettingsPage
{
    title: qsTr("Tutorials")

    Repeater
    {
        model: mainWindow.tutorials?.tutorials
            .filter(tutorial => tutorial.enabled)
            .sort((left, right) => collator.compare(left.title, right.title)) ?? []

        delegate: StyledCheckBox
        {
            readonly property var tutorial: modelData

            width: parent.width
            topPadding: 12
            bottomPadding: 12

            text: NxGlobals.toHtmlEscaped(tutorial.title)
            description: tutorial.description
            checked: appContext.settings.completedTutorials.includes(tutorial.name)

            backgroundRadius: 8
            font.pixelSize: 18

            onToggled:
            {
                const completedTutorials = appContext.settings.completedTutorials
                appContext.settings.completedTutorials = checked
                    ? [...completedTutorials, tutorial.name]
                    : completedTutorials.filter(item => item !== tutorial.name)
            }
        }
    }

    Collator { id: collator }
}
