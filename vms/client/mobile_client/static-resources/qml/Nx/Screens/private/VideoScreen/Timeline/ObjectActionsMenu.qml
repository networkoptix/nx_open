// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Items
import Nx.Ui

import nx.vms.client.mobile.timeline as Timeline

Menu
{
    id: menu

    required property ObjectSelectionSheet selector

    // MultiObjectData owns single object data pointers, while MultiObjectData.perObjectData doesn't.
    // So a copy of the entire MultiObjectData must be held here.
    // TODO: #vkutin Fix that ownership hazard in the future.
    property Timeline.MultiObjectData multiObjectData

    property alias resource: downloadAction.resource
    property int objectsType: Timeline.ObjectsLoader.ObjectsType.motion

    property real minimumDownloadDurationMs: 500

    function adjustPosition(invokerRect /*in parent coords*/, indentFromInvoker)
    {
        d.adjustPosition(invokerRect, indentFromInvoker ?? 8)
    }

    MenuItem
    {
        id: detailsMenuItem

        text: qsTr("Details")
        enabled: detailsAction.enabled && d.objectsData?.length > 0

        onTriggered:
            detailsAction.trigger()
    }

    MenuItem
    {
        id: shareMenuItem

        text: qsTr("Share")

        enabled: shareAction.enabled && d.objectsData?.length > 0
            && menu.objectsType !== Timeline.ObjectsLoader.ObjectsType.motion

        onTriggered:
            d.invokeAction(shareAction, qsTr("Select what to share"))
    }

    MenuItem
    {
        id: downloadMenuItem

        text: qsTr("Download")
        enabled: downloadAction.enabled && d.objectsData?.length > 0

        onTriggered:
            d.invokeAction(downloadAction, qsTr("Select what to download"))
    }

    NxObject
    {
        id: d

        readonly property var objectsData: menu.multiObjectData?.perObjectData ?? []
        property int selectedIndex: 0

        property Action pendingAction: null
        readonly property var singleObjectData: objectsData[selectedIndex]

        Action
        {
            id: detailsAction

            onTriggered:
            {
                if (d.objectsData.length < 1)
                    return

                Workflow.openDetailsScreen(menu.objectsType, d.objectsData)
            }
        }

        // Currently, actions enabled state does not depend on `d.selectedIndex`, because it
        // depends only on the resource which is the same for all objects in `d.objectsData`.

        ShareAction
        {
            id: shareAction

            analyticsMode: menu.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics
            objectData: d.singleObjectData ?? null
        }

        DownloadMediaAction
        {
            id: downloadAction

            positionMs: d.singleObjectData?.startTimeMs ?? 0

            durationMs: Math.max(d.singleObjectData?.durationMs ?? 0,
                menu.minimumDownloadDurationMs)
        }

        Connections
        {
            target: menu.selector

            function onSelected(index)
            {
                menu.selector.close()
                d.selectedIndex = index
                d.executeAction(d.pendingAction)
                d.pendingAction = null
            }

            function onClosed()
            {
                menu.selector.multiObjectData = undefined
            }
        }

        Connections
        {
            target: LayoutController.mainWindow

            function onWidthChanged() { menu.close() }
            function onHeightChanged() { menu.close() }
        }

        function invokeAction(action, selectionPrompt)
        {
            if (d.objectsData?.length === 1)
                d.executeAction(action)
            else
                d.selectObjectForAction(action, selectionPrompt)
        }

        function executeAction(action)
        {
            if (d.singleObjectData && action && action.enabled)
                action.trigger()
        }

        function selectObjectForAction(action, selectionPrompt)
        {
            d.pendingAction = action
            menu.selector.title = selectionPrompt
            menu.selector.multiObjectData = menu.multiObjectData
            menu.selector.open()
        }

        function adjustPosition(invokerRect /*in parent coords*/, indentFromInvoker)
        {
            const parent = menu.parent ?? menu.Overlay.overlay

            if (LayoutController.isPortrait)
            {
                // Offset vertically.
                menu.x = invokerRect.x

                if (invokerRect.y < parent.height - invokerRect.y - invokerRect.height)
                {
                    // Bottom edge.
                    menu.y = invokerRect.y + invokerRect.height + indentFromInvoker
                }
                else
                {
                    // Top edge.
                    const bottom = invokerRect.y - indentFromInvoker
                    menu.y = Qt.binding(() => bottom - menu.height)
                }
            }
            else
            {
                // Offset horizontally.
                menu.y = invokerRect.y

                if (invokerRect.x < parent.width - invokerRect.x - invokerRect.width)
                {
                    // Right edge.
                    menu.x = invokerRect.x + invokerRect.width + indentFromInvoker
                }
                else
                {
                    // Left edge.
                    const right = invokerRect.x - indentFromInvoker
                    menu.x = Qt.binding(() => right - menu.width)
                }
            }
        }
    }

    onMultiObjectDataChanged:
        d.selectedIndex = 0
}
