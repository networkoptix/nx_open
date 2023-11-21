// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls
import nx.vms.client.core
import nx.vms.client.desktop

Button
{
    id: button

    enum State
    {
        Unselected,
        Selected,
        Deactivated
    }

    property bool deactivatable: true
    property bool selectable: true
    property bool accented: false

    property int desiredState: SelectableTextButton.Unselected
    readonly property int state: d.state

    font.pixelSize: FontConfig.normal.pixelSize
    font.weight: Font.Normal

    implicitHeight: Math.max(topPadding + bottomPadding + implicitContentHeight, 24)

    onDesiredStateChanged:
        setState(desiredState)

    function setState(value)
    {
        if (value != SelectableTextButton.State.Unselected
            && value != SelectableTextButton.State.Selected
            && value != SelectableTextButton.State.Deactivated)
        {
            value = SelectableTextButton.State.Unselected
        }

        if (state != value && (value != SelectableTextButton.State.Deactivated || deactivatable)
            && (value != SelectableTextButton.State.Selected || selectable))
        {
            d.state = value
        }
    }

    function deactivate()
    {
        if (deactivatable)
            setState(SelectableTextButton.State.Deactivated)
    }

    // Utility signals.
    signal deactivated()
    signal unselected()
    signal selected()

    onStateChanged:
    {
        switch (state)
        {
            case SelectableTextButton.Deactivated:
                deactivated()
                break

            case SelectableTextButton.Unselected:
                unselected()
                break

            case SelectableTextButton.Selected:
                selected()
                break
        }
    }

    background: Rectangle
    {
        id: rectangle

        implicitWidth: 0
        implicitHeight: 0

        color:
        {
            switch (button.state)
            {
                case SelectableTextButton.State.Unselected:
                {
                    var shift = (button.hovered && !button.down) ? 1 : 0
                    if (accented)
                    {
                        return ColorTheme.transparent(
                            ColorTheme.lighter(ColorTheme.colors.brand_core, shift), 0.4)
                    }
                    else
                    {
                        return ColorTheme.lighter(ColorTheme.colors.dark10, shift)
                    }
                }

                case SelectableTextButton.State.Selected:
                    return ColorTheme.colors.dark5

                case SelectableTextButton.State.Deactivated:
                    return "transparent"
            }
        }

        border.width: 1
        border.color: state == SelectableTextButton.State.Selected
            ? ColorTheme.colors.dark4
            : "transparent"

        radius: 2
    }

    contentItem: Control
    {
        id: content

        leftPadding: (icon.status === Image.Ready) ? 4 : 6
        rightPadding: button.deactivatable ? 0 : 6
        topPadding: 0
        bottomPadding: 0

        contentItem: RowLayout
        {
            spacing: button.spacing

            IconImage
            {
                id: icon

                name: button.icon.name
                color: text.color
                source: button.icon.source
                visible: status === Image.Ready

                Layout.alignment: Qt.AlignVCenter
            }

            Text
            {
                id: text

                text: button.text
                font: button.font
                elide: Text.ElideRight

                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true

                color:
                {
                    switch (button.state)
                    {
                        case SelectableTextButton.State.Unselected:
                            return accented? ColorTheme.colors.light10 : ColorTheme.colors.light14

                        case SelectableTextButton.State.Selected:
                            return ColorTheme.colors.light10

                        case SelectableTextButton.State.Deactivated:
                            return (button.hovered && !button.down)
                                ? ColorTheme.colors.light14
                                : ColorTheme.colors.light16
                    }
                }
            }

            Item
            {
                id: deactivateButtonContainer

                implicitWidth: deactivateButton.implicitWidth
                implicitHeight: deactivateButton.implicitHeight

                visible: button.deactivatable

                ImageButton
                {
                    id: deactivateButton

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    radius: 2

                    icon.source: "image://svg/skin/text_buttons/cross_close_20.svg"
                    icon.color: text.color

                    visible: button.state != SelectableTextButton.State.Deactivated

                    onClicked:
                        button.deactivate()
                }
            }
        }
    }

    opacity: enabled ? 1.0 : 0.3

    spacing: 2

    // Reset paddings to zero default.
    padding: 0
    topPadding: padding
    leftPadding: padding
    rightPadding: padding
    bottomPadding: padding

    onClicked:
        setState(SelectableTextButton.State.Selected)

    onDeactivatableChanged:
        if (!deactivatable && state == SelectableTextButton.State.Deactivated)
            setState(SelectableTextButton.State.Unselected)

    onSelectableChanged:
        if (!selectable && state == SelectableTextButton.State.Selected)
            setState(SelectableTextButton.State.Unselected)

    QtObject
    {
        id: d
        property int state: SelectableTextButton.State.Unselected
    }
}
