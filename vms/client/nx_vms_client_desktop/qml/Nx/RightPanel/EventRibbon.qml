import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

ListView
{
    id: view

    property bool brief: false //< Only resource list and preview.

    boundsBehavior: Flickable.StopAtBounds
    hoverHighlightColor: "transparent"
    spacing: 1
    clip: true

    delegate: Loader
    {
        id: loader

        x: 8
        height: implicitHeight
        width: parent.width - x - 16

        source:
        {
            if (!model || !model.display)
                return "tiles/SeparatorTile.qml"

            if (model.progressValue !== undefined)
                return "tiles/ProgressTile.qml"

            return brief
                ? "tiles/BriefTile.qml"
                : "tiles/InfoTile.qml"
        }

        onVisibleChanged: model.visible = visible
        Component.onCompleted: model.visible = visible
        Component.onDestruction: model.visible = false

        Connections
        {
            target: loader.item
            ignoreUnknownSignals: true

            onCloseRequested:
                view.model.removeRow(index)

            onHoveredChanged:
                view.model.setAutoClosePaused(index, loader.item.hovered)
        }
    }

    add: Transition
    {
        id: addTransition

        NumberAnimation
        {
            property: "opacity"
            from: 0
            to: 1
            duration: 320
            easing.type: Easing.OutQuad
        }
    }

    remove: Transition
    {
        id: removeTransition

        SequentialAnimation
        {
            ScriptAction
            {
                script: removeTransition.ViewTransition.item.enabled = false
            }

            NumberAnimation
            {
                property: "opacity"
                to: 0
                duration: 320
                easing.type: Easing.OutQuad
            }
        }
    }

    displaced: Transition
    {
        id: displayTransition

        NumberAnimation
        {
            property: "y"
            duration: 320
            easing.type: Easing.OutCubic
        }

        // Finish possibly unfinished add animation.
        NumberAnimation
        {
            property: "opacity"
            duration: 320
            to: 1.0
        }
    }

    // Paddings inside viewport.
    header: Item { height: 8; width: parent.width }
    footer: Item { height: 8; width: parent.width }

    onContentYChanged: requestFetchIfNeeded()

    onVisibleChanged:
    {
        requestFetchIfNeeded()

        if (view.model)
            view.model.setLivePaused(!visible)
    }

    Connections
    {
        target: view.model
        onDataNeeded: view.requestFetchIfNeeded()
    }

    function requestFetchIfNeeded()
    {
        if (!view.model || !view.visible)
            return

        if (view.atYEnd || count === 0)
            view.model.setFetchDirection(RightPanel.FetchDirection.earlier)
        else if (view.atYBeginning)
            view.model.setFetchDirection(RightPanel.FetchDirection.later)
        else
            return

        view.model.requestFetch()
    }
}
