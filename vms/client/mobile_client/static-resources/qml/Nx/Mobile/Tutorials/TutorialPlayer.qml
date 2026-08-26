// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import nx.vms.client.mobile

TutorialToolTip
{
    id: control

    property Tutorial tutorial
    property Item defaultTarget: null

    readonly property var steps: enabledSteps(tutorial)
    readonly property TutorialStep currentStep: steps[stepIndex] ?? null
    readonly property var registry: windowContext.interactiveItemRegistry
    readonly property bool applicationActive: Qt.application.state === Qt.ApplicationActive

    signal finished(string name)

    function start(tutorial, defaultTarget)
    {
        const firstStep = enabledSteps(tutorial)[0]
        if (!firstStep || !findTarget(firstStep, defaultTarget))
            return

        control.tutorial = tutorial
        control.defaultTarget = defaultTarget
    }

    function reset()
    {
        tutorial = null
        defaultTarget = null
    }

    function finish()
    {
        if (!tutorial)
            return

        control.finished(tutorial.name)
        reset()
    }

    function enabledSteps(tutorial)
    {
        return tutorial?.steps.filter(step => step.enabled) ?? []
    }

    function findTarget(step, defaultTarget)
    {
        const items = step.anchorName ? registry.items(step.anchorName) : [defaultTarget]

        // Sort items from top to bottom.
        return items
            .filter(item => item && item.visible)
            .sort((a, b) => scenePosition(a).y - scenePosition(b).y)[0] ?? null
    }

    function scenePosition(item)
    {
        return item.mapToItem(null, 0, 0)
    }

    visible: !!target
    target: currentStep ? findTarget(currentStep, defaultTarget) : null
    targetSpacing: 4

    title: currentStep?.title ?? ""
    description: currentStep?.description ?? ""
    stepCount: steps.length
    stepIndex: 0

    onTutorialChanged: stepIndex = 0

    onNextClicked: stepIndex++
    onSkipClicked: finish()
    onDoneClicked: finish()
    onClosed: reset()
    onApplicationActiveChanged: reset()
}
