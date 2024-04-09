// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

import "private"

Control
{
    id: control

    property var model: null //< Must have the "count" property.

    property string textRole: "text"
    property string decorationRole: "decorationPath"
    property string checkedRole: "checked"
    property string enabledRole: "enabled"

    property bool stickyPopup: false //< Popup follows bottom border when control height changes.
    readonly property color highlightColor: ColorTheme.colors.yellow_d1
    property alias placeholder: placeholder
    property alias maxRows: flow.rowLimit
    property var maxItems

    property alias listView: listView

    implicitWidth: 208

    focusPolicy: Qt.TabFocus
    onEnabledChanged: popup.close()

    onActiveFocusChanged:
    {
        if (activeFocus)
            popup.close()
    }

    Keys.onSpacePressed: control.openPopup()
    Keys.onTabPressed: control.nextItemInFocusChain().forceActiveFocus()

    FocusFrame
    {
        anchors.fill: parent
        anchors.margins: 1
        visible: control.visualFocus
    }

    background: Rectangle
    {
        color: popup.visible
            ? ColorTheme.colors.dark4
            : mouseArea.containsMouse
                ? ColorTheme.colors.dark6
                : ColorTheme.colors.dark5

        border.width: 1
        border.color: ColorTheme.darker(color, 1)

        opacity: control.enabled ? 1.0 : 0.3
    }

    contentItem: MouseArea
    {
        id: mouseArea

        hoverEnabled: control.enabled

        implicitHeight: layout.implicitHeight
        implicitWidth: layout.implicitWidth
        baselineOffset: layout.baselineOffset

        onClicked: openPopup()

        Text
        {
            id: placeholder

            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter

            text: qsTr("Select")
            visible: valuesModel.count == 0
            opacity: control.enabled ? 1.0 : 0.3
            color: ColorTheme.colors.light16
            font: Qt.font({pixelSize: 14, weight: Font.Normal})
        }

        RowLayout
        {
            id: layout

            spacing: 2
            baselineOffset: flow.baselineOffset

            anchors.fill: parent

            LimitedFlow
            {
                id: flow

                spacing: 2

                leftPadding: 2
                rightPadding: 2
                topPadding: 3
                bottomPadding: 3

                Layout.fillWidth: true

                sourceModel: SortFilterProxyModel
                {
                    id: valuesModel

                    filterRoleName: checkedRole
                    filterRegularExpression: /true/
                    sourceModel: control.model
                }

                delegate: MultiSelectionFieldItem
                {
                    width: Math.min(implicitWidth, flow.width)
                    text: model[textRole] ?? ""
                    iconSource: model[decorationRole] ?? ""
                    removable: model[enabledRole] ?? true
                    onRemoveClicked: model[checkedRole] = !model[checkedRole]
                }

                MultiSelectionFieldItem
                {
                    text: qsTr(`${flow.remaining} more`)
                    color: ColorTheme.colors.light10
                    backgroundColor: ColorTheme.colors.dark7
                    removable: false
                }
            }

            ColoredImage
            {
                Layout.margins: 6
                Layout.alignment: Qt.AlignTop

                sourcePath: "image://skin/user_settings/arrow_down.svg"
                sourceSize: Qt.size(16, 16)
            }
        }
    }

    Popup
    {
        id: popup

        property int maxVisibleItems: 7 //< Maximum amount of items to be displayed in the list.

        implicitWidth: control.width
        implicitHeight: contentItem.implicitHeight

        leftPadding: 0
        rightPadding: 0
        topPadding: 2
        bottomPadding: 2

        background: Rectangle
        {
            color: ColorTheme.colors.dark13
            radius: 1
        }

        PopupShadow { source: popup.background }

        contentItem: Control //< Required for PopupShadow.
        {
            contentItem: ColumnLayout
            {
                spacing: 0

                MultiSelectionSearchField
                {
                    id: searchField

                    Layout.margins: 8
                    Layout.fillWidth: true

                    visible: control.model.count > popup.maxVisibleItems

                    KeyNavigation.backtab: control
                    KeyNavigation.tab: listView

                    onAccepted:
                    {
                        if (listView.count == 1)
                        {
                            listView.itemAtIndex(0).setChecked(true)
                            popup.close()
                        }
                    }
                }

                ListView
                {
                    id: listView

                    property var currentSearchRegExp: searchField.text
                        ? new RegExp(
                            `(${NxGlobals.makeSearchRegExpNoAnchors(searchField.text)})`, 'i')
                        : ""

                    model: SortFilterProxyModel
                    {
                        sourceModel: control.model
                        filterRoleName: textRole
                        filterRegularExpression: listView.currentSearchRegExp
                    }

                    visible: count > 0
                    currentIndex: 0
                    keyNavigationWraps: true
                    keyNavigationEnabled: true
                    clip: true

                    scrollBar.width: 8
                    scrollBar.thumbColor: ColorTheme.colors.dark16

                    Layout.fillWidth: true
                    Layout.preferredHeight: contentHeight

                    // Some initial maximum height.
                    Layout.maximumHeight: 100

                    delegate: MultiSelectionListItem
                    {
                        text: highlightMatchingText(model[textRole])
                        checked: model[checkedRole]
                        iconSource: model[decorationRole] ?? ""
                        current: listView.activeFocus && ListView.isCurrentItem

                        property bool canBeChecked: !maxItems || valuesModel.count < maxItems
                        enabled: (model[enabledRole] ?? true) && (checked || canBeChecked)

                        width: listView.width
                            - (listView.scrollBar.visible ? listView.scrollBar.width : 0)

                        onClicked: setChecked(checked)

                        onYChanged:
                        {
                            // Set the maximum height when the position of the item is known.
                            if (index == popup.maxVisibleItems - 1)
                                listView.Layout.maximumHeight = y + height
                        }

                        function setChecked(value)
                        {
                            model[checkedRole] = value
                        }

                        Keys.onTabPressed:
                        {
                            popup.close()
                            control.nextItemInFocusChain().forceActiveFocus()
                        }

                        Keys.onSpacePressed: setChecked(!checked)
                    }
                }

                Text
                {
                    text: qsTr("Nothing found")
                    visible: searchField.visible && listView.count == 0
                    color: ColorTheme.colors.light16
                    font: Qt.font({pixelSize: 14, weight: Font.Normal})
                    wrapMode: Text.WordWrap

                    topPadding: 16
                    bottomPadding: 24
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }
        }

        onVisibleChanged:
        {
            if (searchField.visible)
                searchField.forceActiveFocus()
        }

        onImplicitHeightChanged:
        {
            setupPosition(/*initial*/ true)
        }

        function setupPosition(initial)
        {
            const getYPosition = (initial) =>
            {
                let showAbove = false

                // Open popup at line border below mouse click.
                let popupY = control.height

                const popupBottom = popup.parent.mapToGlobal(0, popupY + popup.implicitHeight)

                // Place popup above mouse click if the popup does not fit within window.
                const window = control.Window.window
                const windowBottomY = window.y + window.height
                if (!popup.visible || initial)
                    showAbove = popupBottom.y > windowBottomY

                if (showAbove)
                    popupY -= (popup.implicitHeight + control.height)

                return popupY
            }

            popup.y = control.stickyPopup ? Qt.binding(getYPosition) : getYPosition(initial)
        }
    }

    function openPopup()
    {
        popup.setupPosition()
        popup.open()
    }

    function highlightMatchingText(text)
    {
        if (listView.currentSearchRegExp)
            return NxGlobals.highlightMatch(text, listView.currentSearchRegExp, highlightColor)

        return NxGlobals.toHtmlEscaped(text)
    }
}
