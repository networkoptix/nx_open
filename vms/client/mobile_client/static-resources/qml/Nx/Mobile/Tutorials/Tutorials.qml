// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    id: control

    property list<Tutorial> tutorials:
    [
        CameraSwitcherTutorial {},
        ObjectTypesTutorial {},
        PanelButtonTutorial {},
        SoftTriggersTutorial {},
        TimelinePreviewTutorial {},
        TimelineZoomTutorial {}
    ]

    Instantiator
    {
        model: tutorials.filter(tutorial =>
            !appContext.settings.completedTutorials.includes(tutorial.name) && tutorial.enabled)

        delegate: Connections
        {
            readonly property Tutorial tutorial: modelData

            target: tutorial.trigger

            function onActivated(item)
            {
                if (!player.tutorial)
                    player.start(tutorial, item)
            }
        }
    }

    TutorialPlayer
    {
        id: player

        onFinished: (tutorialName) =>
        {
            appContext.settings.completedTutorials =
                [...appContext.settings.completedTutorials, tutorialName]
        }
    }
}
