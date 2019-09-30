import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4 as T

import Nx 1.0

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

    readonly property int state: d.state

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

    contentItem: Item
    {
        id: content

        implicitWidth: row.implicitWidth + (button.deactivatable ? deactivateButton.width : 0)
        implicitHeight: Math.max(row.implicitHeight, deactivateButton.height)

        Row
        {
            id: row

            anchors.verticalCenter: parent.verticalCenter

            leftPadding: icon.visible ? 4 : 6
            rightPadding: button.deactivatable ? spacing : 6
            spacing: button.spacing

            T.IconImage
            {
                id: icon

                name: button.icon.name
                color: text.color
                source: button.icon.source
                anchors.verticalCenter: parent.verticalCenter
            }

            Text
            {
                id: text

                text: button.text
                anchors.verticalCenter: parent.verticalCenter

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
        }

        CloseButton
        {
            id: deactivateButton

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            icon.color: text.color
            visible: button.deactivatable && button.state != SelectableTextButton.State.Deactivated

            onClicked:
                button.setState(SelectableTextButton.State.Deactivated)
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
