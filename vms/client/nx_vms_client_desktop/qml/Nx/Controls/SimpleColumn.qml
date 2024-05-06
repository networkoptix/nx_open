// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQml

import Nx
import Nx.Core

import nx.vms.client.core

/**
 * Standard Column defers positioning of its items to avoid frequent repositioning.
 * This component ensures immediate positioning as it builds rigid bindings between its children.
 */
Item
{
    id: column

    property real leftPadding: 0
    property real topPadding: 0
    property real rightPadding: 0
    property real bottomPadding: 0

    property real spacing: 0

    readonly property alias relevantChildren: d.relevantChildren

    readonly property rect relevantChildrenRect:
    {
        const defaultRect = Qt.rect(leftPadding, topPadding, 0, 0)

        return Array.prototype.reduce.call(relevantChildren,
            function(currentRect, item)
            {
                const left = Math.min(currentRect.left, item.x)
                const top = Math.min(currentRect.top, item.y)
                const right = Math.max(currentRect.right, item.x + item.width)
                const bottom = Math.max(currentRect.bottom, item.y + item.height)
                return Qt.rect(left, top, right - left, bottom - top)
            },
            defaultRect)
    }

    implicitWidth: relevantChildrenRect.right + rightPadding
    implicitHeight: relevantChildrenRect.bottom + bottomPadding

    Instantiator
    {
        id: d

        property var relevantChildren: []
        model: d.relevantChildren.length

        delegate: Component
        {
            NxObject
            {
                Binding
                {
                    target: d.relevantChildren[index] ?? null
                    property: "x"
                    value: column.leftPadding
                }

                Binding
                {
                    target: d.relevantChildren[index] ?? null
                    property: "y"

                    value:
                    {
                        const previous = d.relevantChildren[index - 1]
                        return previous
                            ? (previous.y + previous.height + column.spacing)
                            : column.topPadding
                    }
                }
            }
        }
    }

    Binding
    {
        target: d
        property: "relevantChildren"
        value: Array.prototype.filter.call(column.visibleChildren,
            child => NxGlobals.isRelevantForPositioners(child))
        when: column.visible
        restoreMode: Binding.RestoreNone
    }
}
