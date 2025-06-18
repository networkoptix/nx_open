// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick as Q
import QtQuick.Controls
import QtQuick.Templates as T

import Nx.Core
import Nx.Core.Controls

import "private"

// TODO #ynikitenkov Add "support text" functionality support.
// Note: On MacOS ComboBox can't get focus by the keyboard (TAB) until it is set in OS settings.
// See QTBUG-39647.
T.ComboBox
{
    id: control

    property string popupTitle
    property alias backgroundMode: fieldBackground.mode
    property alias labelText: fieldBackground.labelText
    property alias supportText: fieldBackground.supportText
    property alias errorText: fieldBackground.errorText

    implicitWidth: 268
    implicitHeight: 56

    textRole: "display"
    font.pixelSize: 16
    font.weight: 500

    spacing: 8

    topPadding: 30
    leftPadding: 12
    rightPadding: indicatorImage.visible
        ? control.spacing + indicatorImage.width + indicatorImage.anchors.rightMargin
        : indicatorImage.anchors.rightMargin

    background: FieldBackground
    {
        id: fieldBackground

        owner: control
        compactLabelMode: (!!contentLoader.item?.text ?? false) || control.activeFocus
    }

    onActivated:
    {
        fieldBackground.errorText = ""
        accepted()
    }

    onEditTextChanged: d.updateCurrentIndex()

    Q.Connections
    {
        // Note: plain models don't have the signals below.
        target: (model && !NxGlobals.isSequence(model))
            ? model
            : null

        ignoreUnknownSignals: true

        function onModelReset() { d.updateCurrentIndex() }
        function onRowsInserted() { d.updateCurrentIndex() }
        function onRowsRemoved() { d.updateCurrentIndex() }
        function onRowsMoved() { d.updateCurrentIndex() }
    }

    // Workaround: prevents too early focus on press event. Otherwise on mobile devices
    // user can see keyboard unexpectedly going up and down.
    // We have to use this mouse area workaround to preserve natural behavior similar to the
    // other controls like text edits.
    component FocusWorkaround: Q.MouseArea
    {
        id: focusWorkaround

        property var extraAction

        onReleased:
            (mouse) =>
            {
                if (!d.isMouseEventInsideControl(mouse))
                    return

                control.forceActiveFocus()
                if (extraAction)
                    extraAction()
            }
    }

    // Prevents popup showing and early focus on click to the label area.
    FocusWorkaround
    {
        id: labelFocusAndClickWorkaround

        visible: control.editable
        height: topPadding
        width: contentItem.width + leftPadding
    }

    // Prevents early focus in the indicator area and shows popup if needed.
    FocusWorkaround
    {
        id: indicatorFocusWorkaround

        anchors.right: parent.right
        width: editable
            ? parent.width - contentItem.width - leftPadding
            : parent.width
        height: parent.height

        extraAction: () => { control.popup.open() }
    }

    // Prevents early focus in the left padding area.
    FocusWorkaround
    {
        id: leftPaddingFocusWorkaround

        width: leftPadding
        height: parent.height
    }

    contentItem: Q.Loader
    {
        id: contentLoader

        focus: true
        sourceComponent: control.editable
            ? textInputContentItemComponent
            : textContentItemComponent

        Q.Component
        {
            id: textInputContentItemComponent

            // Using TextField instead of TextInput as it works correctly with focus - it
            // gets it on mouse release or proper movement, not from pressed event (like TextInput).
            TextField
            {
                focus: true
                clip: true
                font: control.font
                color: enabled
                    ? ColorTheme.colors.light4
                    : ColorTheme.transparent(ColorTheme.colors.light4, 0.3)

                text: control.editText
                topPadding: 0
                leftPadding: 0
                rightPadding: 0
                onTextChanged:
                {
                    control.errorText = ""
                    if (control.editText !== text)
                        control.editText = text
                }

                inputMethodHints: control.inputMethodHints

                onAccepted: control.accepted()

                background: null
            }
        }

        Q.Component
        {
            id: textContentItemComponent

            Q.Text
            {
                focus: true
                font: control.font
                color: enabled
                    ? ColorTheme.colors.light4
                    : ColorTheme.transparent(ColorTheme.colors.light4, 0.3)
                elide: Q.Text.ElideRight

                text: control.displayText
            }
        }
    }

    indicator: ColoredImage
    {
        id: indicatorImage

        visible: control.count

        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter

        opacity: enabled ? 1 : 0.3

        sourcePath: control.popup.visible
            ? "image://skin/20x20/Outline/arrow_up.svg?primary=light4"
            : "image://skin/20x20/Outline/arrow_down.svg?primary=light4"
        sourceSize: Qt.size(20, 20)
    }

    delegate: PopupItemDelegate
    {
        selected: control.currentIndex === index
        text:
        {
            const data = modelData ?? model
            return control.textRole
                ? data[control.textRole]
                : data
        }
    }

    popup: Popup
    {
        readonly property int kMaxElementsCount: 4

        width: control.width
        height:
        {
            const paddings = topPadding + bottomPadding
            const maxHeight = kMaxElementsCount * 48
                + (kMaxElementsCount - 1) * listView.spacing
                + (listView.headerItem?.height ?? 0)
                + (listView.footerItem?.height ?? 0)
            return Math.min(contentItem.implicitHeight, maxHeight) + paddings
        }

        modal: true
        padding: 0
        topPadding: 8
        bottomPadding: 8
        background: Q.Rectangle
        {
            color: ColorTheme.colors.dark10
            radius: 8

            Q.Rectangle
            {
                y: parent.radius
                width: parent.width
                height: parent.height - parent.radius * 2
                color: ColorTheme.colors.dark12
            }
        }

        contentItem: Q.ListView
        {
            id: listView

            clip: true

            snapMode: Q.ListView.NoSnap
            boundsBehavior: Q.Flickable.StopAtBounds
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null

            spacing: 1

            header: PopupItemDelegate
            {
                text: control.popupTitle
                visible: text.length
                textColor: ColorTheme.colors.light14
                height: visible ? 49 : 0

                Q.Rectangle
                {
                    y: parent.height - height
                    width: parent.width
                    height: 1
                    color: ColorTheme.colors.dark12
                }
            }

            footer: PopupItemDelegate
            {
                text: qsTr("Cancel")
                height: 49

                Q.Rectangle
                {
                    width: parent.width
                    height: 1
                    color: ColorTheme.colors.dark12
                }

                Q.MouseArea
                {
                    anchors.fill: parent

                    onClicked: control.popup.close()
                }
            }
        }

        onOpened:
        {
            if (!indicatorImage.visible)
                close()
        }
    }

    NxObject
    {
        id: d

        property bool updating: false

        function updateCurrentIndex()
        {
            if (updating)
                return

            d.updating = true

            const currentEditText = control.editText
            const newIndex =
                (() =>
                {
                    const trimmed = currentEditText.trim()
                    for (let i = 0; i != control.count; ++i)
                    {
                        if (control.textAt(i) === trimmed)
                            return i
                    }
                    return -1
                })()

            control.currentIndex = newIndex

            // Settings current index to -1 resets edit text so we have to restore it.
            if (control.currentIndex === -1)
                control.editText = currentEditText

            d.updating = false
        }

        function isMouseEventInsideControl(mouse)
        {
            return mouse.x >= 0 && mouse.x <= control.width
                && mouse.y >= 0 && mouse.y <= control.height
        }
    }
}
