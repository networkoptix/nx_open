// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core
import Nx.Controls
import Nx.Instruments
import Nx.Items

import nx.client.core
import nx.vms.client.desktop

import "../Components"

Item
{
    id: control

    required property AccessSubjectEditingContext editingContext

    readonly property int columnCount: availableAccessRightDescriptors.length

    readonly property int kRowHeight: 28
    readonly property int kColumnWidth: 64

    property var buttonBox

    property bool automaticDependenciesSwitchVisible: false

    property bool editingEnabled: true

    readonly property var availableAccessRightDescriptors:
        Array.prototype.slice.call(AccessRightsList.items) //< Deep copy to JS for optimization.

    readonly property var availableAccessRights:
        Array.prototype.map.call(availableAccessRightDescriptors, item => item.accessRight)

    Item
    {
        id: accessRightsHeader

        anchors.right: control.right
        anchors.rightMargin: 9

        height: 64
        width: control.columnCount * control.kColumnWidth

        property int hoveredAccessRight: tree.hoveredCell
            ? tree.hoveredCell.accessRight
            : 0

        Row
        {
            Repeater
            {
                model: control.availableAccessRightDescriptors

                AccessRightsHeaderItem
                {
                    id: accessRightItem

                    height: accessRightsHeader.height
                    width: control.kColumnWidth
                    icon: modelData.icon

                    color: accessRightsHeader.hoveredAccessRight == modelData.accessRight && enabled
                        ? ColorTheme.colors.light4
                        : ColorTheme.colors.light10

                    enabled: control.editingEnabled && !frameSelector.dragging

                    GlobalToolTip.text: modelData.description

                    readonly property int index: Positioner.index
                    readonly property int accessRight: modelData.accessRight
                }
            }
        }
    }

    Rectangle
    {
        id: permissionsBackground

        anchors
        {
            left: accessRightsHeader.left
            right: tree.right
            top: tree.top
            bottom: tree.bottom
        }

        color: control.editingEnabled ? ColorTheme.colors.dark4 : ColorTheme.colors.dark7
    }

    ResourceSelectionTree
    {
        id: tree

        property var hoveredRow: null
        property var hoveredCell: hoveredRow ? hoveredRow.hoveredCell : null

        readonly property real scrollBarWidth: scrollBarVisible ? scrollBar.width : 0

        property var currentSearchRegExp: null

        onFilterTextChanged:
        {
            // Search happens in an escaped text, so also escape filter text here.
            const escaped = tree.filterText
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")

            tree.currentSearchRegExp = escaped
                ? new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(escaped)})`, 'i')
                : ""
        }

        anchors.fill: parent
        anchors.topMargin: accessRightsHeader.height - 4
        anchors.rightMargin: 8 - scrollBarWidth
        anchors.leftMargin: 16

        expandsOnDoubleClick: false
        hoverHighlightColor: "transparent"
        showResourceStatus: false
        clipDelegates: false
        topMargin: 0

        resourceTypes:
        {
            const kAllTypes = ResourceTree.ResourceFilter.camerasAndDevices
                | ResourceTree.ResourceFilter.integrations
                | ResourceTree.ResourceFilter.webPages
                | ResourceTree.ResourceFilter.layouts
                | ResourceTree.ResourceFilter.healthMonitors
                | ResourceTree.ResourceFilter.videoWalls

            return filterButton.resourceTypeFilter || kAllTypes
        }

        topLevelNodesPolicy: ResourceSelectionModel.TopLevelNodesPolicy.showAlways
        webPagesAndIntegrationsCombined: true

        filterText: searchField.text

        Binding
        {
            target: tree.model
            property: "resourceFilter"
            value: control.editingContext && control.editingContext.accessibleResourcesFilter
            when: control.editingContext && filterButton.withPermissionsOnly
        }

        delegate: ResourceAccessDelegate
        {
            id: rowAccess

            editingContext: control.editingContext

            showExtraInfo: tree.model.extraInfoRequired
                || tree.model.isExtraInfoForced(resource)

            selectionMode: tree.selectionMode
            showResourceStatus: tree.showResourceStatus
            columnWidth: control.kColumnWidth
            implicitHeight: control.kRowHeight
            editingEnabled: control.editingEnabled
            automaticDependencies: automaticDependenciesSwitch.checked
            accessRightsList: control.availableAccessRights
            highlightRegExp: tree.currentSearchRegExp
            frameSelectionMode: frameSelector.currentMode

            frameSelectionRect:
            {
                if (!frameSelector.dragging || !control.editingEnabled)
                    return Qt.rect(0, 0, 0, 0)

                return rowAccess.mapFromItem(tree, tree.mapFromItem(selectionArea,
                    selectionArea.frameRect))
            }

            onHoveredCellChanged:
            {
                if (hoveredCell)
                    tree.hoveredRow = rowAccess
                else if (tree.hoveredRow == rowAccess)
                    tree.hoveredRow = null
            }

            onFrameInitializationResponse: (cell) =>
            {
                if (frameSelector.currentMode == ResourceAccessDelegate.FrameInitialization)
                    frameSelector.currentMode = frameSelector.modeFromCell(cell)
            }

            Connections
            {
                target: frameSelector
                function onCanceled() { rowAccess.hoveredCell = null }
            }
        }

        MouseArea
        {
            id: selectionArea

            x: accessRightsHeader.x - tree.x + 1
            y: 1
            width: accessRightsHeader.width
            height: Math.max(tree.height - 1, 0)

            hoverEnabled: false
            propagateComposedEvents: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            property point start

            readonly property point current: Qt.point(
                frameSelector.position.x,
                frameSelector.position.y - tree.contentItem.y)

            readonly property point topLeft: Qt.point(
                Math.max(0, Math.min(start.x, current.x)),
                Math.max(0, Math.min(start.y, current.y)) + tree.contentItem.y);

            readonly property point bottomRight: Qt.point(
                Math.min(width - 1, Math.max(start.x, current.x)),
                Math.min(Math.max(height, tree.contentItem.height) - 1,
                    Math.max(start.y, current.y)) + tree.contentItem.y);

            readonly property rect frameRect: Qt.rect(
                topLeft.x,
                topLeft.y,
                bottomRight.x - topLeft.x,
                bottomRight.y - topLeft.y)

            onCanceled: //< Called when another item grabs mouse exclusively.
                frameSelector.cancel()

            SimpleDragInstrument
            {
                id: frameSelector

                item: selectionArea
                enabled: control.editingEnabled

                readonly property bool selectionControlledByCtrl:
                    LocalSettings.iniConfigValue("permissionFrameSelectionControlledByCtrl")

                readonly property bool ctrlPressed: KeyboardModifiers.ctrlPressed

                property int currentMode: ResourceAccessDelegate.NoFrameOperation

                function modeFromCell(cell)
                {
                    return cell.isToggledOn()
                        ? ResourceAccessDelegate.FrameUnselection
                        : ResourceAccessDelegate.FrameSelection
                }

                onStarted:
                {
                    if (selectionControlledByCtrl)
                    {
                        currentMode = Qt.binding(() => frameSelector.ctrlPressed
                            ? ResourceAccessDelegate.FrameUnselection
                            : ResourceAccessDelegate.FrameSelection)
                    }
                    else
                    {
                        currentMode = tree.hoveredCell && tree.hoveredCell.frameSelected
                            ? modeFromCell(tree.hoveredCell)
                            : ResourceAccessDelegate.FrameInitialization
                    }

                    selectionArea.start = Qt.point(
                        pressPosition.x,
                        pressPosition.y - tree.contentItem.y)
                }

                onFinished:
                {
                    if (!control.editingContext
                        || currentMode == ResourceAccessDelegate.FrameInitialization
                        || selectionArea.frameRect.width === 0
                        || selectionArea.frameRect.height === 0)
                    {
                        currentMode = ResourceAccessDelegate.NoFrameOperation
                        return
                    }

                    const nextCheckValue = currentMode == ResourceAccessDelegate.FrameSelection
                    currentMode = ResourceAccessDelegate.NoFrameOperation

                    const rect = Qt.rect(
                        selectionArea.frameRect.left,
                        selectionArea.frameRect.top - tree.contentItem.y,
                        selectionArea.frameRect.width,
                        selectionArea.frameRect.height)

                    const firstRow = Math.floor(rect.top / control.kRowHeight)
                    const lastRow = Math.floor(rect.bottom / control.kRowHeight)
                    const firstColumn = Math.floor(rect.left / control.kColumnWidth)
                    const lastColumn = Math.floor(rect.right / control.kColumnWidth)

                    let accessRights = 0
                    for (let column = firstColumn; column <= lastColumn; ++column)
                        accessRights |= control.availableAccessRights[column];

                    const indexes = tree.rowIndexes(firstRow, lastRow)
                    const resources = indexes.map(index => NxGlobals.modelData(index, "resource"))
                        .filter(resource => !!resource)

                    control.editingContext.modifyAccessRights(resources, accessRights,
                        nextCheckValue, automaticDependenciesSwitch.checked)
                }

                onCanceled:
                {
                    currentMode = ResourceAccessDelegate.NoFrameOperation
                    tree.hoveredRow = null
                }
            }

            Binding
            {
                when: frameSelector.dragging
                target: tree.autoScroller
                property: "velocity"

                value:
                {
                    const rect = Qt.rect(
                        0, -tree.contentItem.y, selectionArea.width, selectionArea.height)

                    return ProximityScrollHelper.velocity(rect,
                        Geometry.bounded(selectionArea.current, rect))
                }
            }

            Rectangle
            {
                id: selectionFrame

                visible: frameSelector.currentMode != ResourceAccessDelegate.NoFrameOperation
                z: 2

                color: ColorTheme.transparent(border.color, 0.2)

                border.color: frameSelector.currentMode != ResourceAccessDelegate.FrameUnselection
                    ? ColorTheme.colors.brand_core
                    : ColorTheme.colors.light16

                x: selectionArea.frameRect.left
                y: selectionArea.frameRect.top
                width: selectionArea.frameRect.width
                height: selectionArea.frameRect.height
            }

            Popup
            {
                id: frameHint

                visible: frameSelector.currentMode != ResourceAccessDelegate.NoFrameOperation
                    && frameSelector.selectionControlledByCtrl

                x: Math.min(selectionFrame.x, selectionArea.width - width)
                y: selectionFrame.y - height

                background: null
                padding: 0

                contentItem: Text
                {
                    font.pixelSize: 10
                    font.weight: Font.Light

                    color: selectionFrame.border.color

                    text:
                    {
                        if (frameSelector.currentMode == ResourceAccessDelegate.FrameUnselection)
                            return qsTr("Drag to uncheck")

                        return qsTr("Drag to check, with %1 to uncheck",
                            "%1 will be substituted with a name of a key").arg(
                                NxGlobals.modifierName(Qt.ControlModifier))
                    }
                }
            }
        }
    }

    Item
    {
        anchors.left: parent.left
        anchors.top: accessRightsHeader.bottom
        anchors.bottom: parent.bottom

        width: searchField.width

        visible: tree.empty

        Placeholder
        {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -(searchField.y + searchField.height)

            imageSource: "image://svg/skin/user_settings/no_resources.svg"

            text: qsTr("No resources")
            additionalText: qsTr("Try changing search criteria")
        }
    }

    SearchField
    {
        id: searchField

        anchors.left: parent.left
        anchors.bottom: tree.top
        anchors.right: filterButton.left
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        anchors.rightMargin: 4
    }

    ResourceFilterButton
    {
        id: filterButton

        anchors.top: searchField.top
        anchors.bottom: searchField.bottom
        anchors.right: accessRightsHeader.left
        anchors.rightMargin: 16
        width: 32

        editingEnabled: control.editingEnabled

        onWithPermissionsOnlyChanged:
        {
            if (editingContext)
                editingContext.resetAccessibleResourcesFilter()
        }
    }

    // Lines and shadow.

    Rectangle
    {
        x: permissionsBackground.x
        y: permissionsBackground.y
        width: permissionsBackground.width
        height: 1
        color: ColorTheme.colors.dark5
    }

    Rectangle
    {
        x: permissionsBackground.x
        width: 1
        height: permissionsBackground.y + permissionsBackground.height
        color: ColorTheme.colors.dark5
    }

    Rectangle
    {
        x: permissionsBackground.x - 1
        width: 1
        height: permissionsBackground.y + permissionsBackground.height
        color: ColorTheme.colors.dark8
    }

    Rectangle
    {
        y: permissionsBackground.y
        width: control.width
        height: 16
        opacity: MathUtils.bound(0.0, tree.contentY / height, 1.0)

        gradient: Gradient
        {
            GradientStop { position: 0.0; color: ColorTheme.transparent("black", 0.4) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Item
    {
        // Covers dialog button box.

        anchors.top: parent.bottom

        height: control.buttonBox.height
        width: parent.width

        Switch
        {
            id: automaticDependenciesSwitch

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter

            checked: true
            visible: control.editingEnabled && control.automaticDependenciesSwitchVisible

            text: qsTr("Automatically add dependent access rights")
        }
    }

    Shortcut
    {
        sequence: "Ctrl+Shift+A"
        enabled: !control.automaticDependenciesSwitchVisible

        onActivated:
            control.automaticDependenciesSwitchVisible = true
    }

    Shortcut
    {
        sequence: "Esc"
        enabled: frameSelector.dragging

        onActivated:
            frameSelector.cancel()
    }
}
