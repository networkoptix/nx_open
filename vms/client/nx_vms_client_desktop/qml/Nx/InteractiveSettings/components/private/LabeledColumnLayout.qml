// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import nx.vms.client.desktop

/**
 * Labeled Column Layout for items with explicit labels (see LabeledItem). An item in layout can
 * have the following properties:
 * - labelText: The text of the label that is placed next to the item.
 * - isGroup: If true, the item will have additional spacings.
 * - fillWidth: If true, the item will be as wide as possible.
 * - stretchable: If true, the item may be stretched if it's the only one.
 *
 * Redirects all child items to the inner layout (see layoutItems).
 */
LayoutBase
{
    id: layout

    /** The control which contains this layout. */
    property var layoutControl: this

    // Possible parent layout.
    readonly property var parentLayout: layoutControl && layoutControl.parent

    readonly property real parentLayoutShift:
        (parentLayout && width > 0) ? mapToItem(parentLayout, 0, 0).x : 0 //< Depends on width.

    readonly property real labelWidthHint:
        (parentLayout && parentLayout.labelWidth) ? parentLayout.labelWidth - parentLayoutShift : 0

    readonly property real kLabelWidthFraction: 0.3
    readonly property real labelWidth: labelWidthHint < 64
        ? width * kLabelWidthFraction
        : labelWidthHint

    /** Preferred layout height, items may be stretched to fill space. */
    property var preferredHeight: undefined

    readonly property bool stretchable:
        layoutItems.length === 1 && !!layoutItems[0].stretchable && preferredHeight !== undefined

    spacing: 8
    implicitWidth: 400
    implicitHeight: stretchable && preferredHeight ? preferredHeight : contentItem.implicitHeight

    onItemAdded: (item) => items.append({item: item, isBaselineAligned: false})

    contentItem: GridLayout
    {
        columns: 2
        columnSpacing: 8
        rowSpacing: layout.spacing

        ListModel
        {
            id: items
        }

        Repeater
        {
            id: labels

            model: items

            Label
            {
                // isBaselineAligned is calculated and shared using the model, so fields can use it.
                property bool isBaselineAligned: lineCount === 1 && item.baselineOffset > 0
                property int verticalAlignment: isBaselineAligned ? Qt.AlignBaseline : Qt.AlignTop
                onIsBaselineAlignedChanged: model.isBaselineAligned = isBaselineAligned

                Layout.column: 0
                Layout.row: index
                Layout.alignment: Qt.AlignRight | verticalAlignment
                Layout.preferredWidth: layout.labelWidth

                property bool hasExplicitLabel: item.labelText !== undefined
                visible: item.labelText !== undefined && item.opacity > 0
                text: item.labelText ?? ""
                horizontalAlignment: Text.AlignRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                elide: Text.ElideRight

                contextHintText: item.description ?? ""
                GlobalToolTip.text: truncated ? text : null
            }
        }

        Repeater
        {
            id: fields

            model: items

            BottomPaddedItem
            {
                // Shared properties (items can use it).
                property real labelWidth: layout.labelWidth

                property int verticalAlignment: isBaselineAligned ? Qt.AlignBaseline : Qt.AlignTop

                property bool hasExplicitLabel: item.labelText !== undefined
                Layout.column: hasExplicitLabel ? 1 : 0
                Layout.columnSpan: hasExplicitLabel ? 1 : 2
                Layout.row: index
                Layout.alignment: Qt.AlignLeft | verticalAlignment
                Layout.fillWidth: item.fillWidth ?? hasExplicitLabel
                Layout.fillHeight: layout.stretchable

                contentItem: item
                nextItem: index + 1 < items.count ? items.get(index + 1).item : null
                visible: item.opacity > 0

                onContentItemChanged: //< Force parent change (for items with already set parent).
                    contentItem.parent = this
            }
        }
    }
}
