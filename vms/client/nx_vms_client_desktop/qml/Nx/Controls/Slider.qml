// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core.Controls

import nx.vms.client.desktop

Slider
{
    id: slider

    focusPolicy: Qt.TabFocus

    handleSize: 16
    grooveWidth: 4

    grooveBorderColor: ColorTheme.colors.dark5

    filledGrooveColor: enabled
        ? (hovered ? ColorTheme.colors.light15 : ColorTheme.colors.light16)
        : ColorTheme.colors.dark15

    handleColor: pressed ? ColorTheme.colors.dark10 : ColorTheme.colors.dark7

    handleBorderColor: enabled
        ? ((handleHovered || pressed) ? ColorTheme.colors.light12 : ColorTheme.colors.light16)
        : ColorTheme.colors.dark15

    readonly property alias handleHovered: hoverHandler.hovered

    FocusFrame
    {
        id: focusFrame

        anchors.fill: parent
        visible: slider.visualFocus
    }

    Keys.onPressed:
    {
        event.accepted = true
        switch (event.key)
        {
            case Qt.Key_Home:
                value = from
                break

            case Qt.Key_End:
                value = to
                break

            case Qt.Key_PageDown:
                value -= pageSize
                break

            case Qt.Key_PageUp:
                value += pageSize
                break

            case Qt.Key_Left:
            case Qt.Key_Down:
                decrease()
                break;

            case Qt.Key_Right:
            case Qt.Key_Up:
                increase()
                break;

            default:
                event.accepted = false
                break
        }

        if (event.accepted)
            moved()
    }

    HoverHandler
    {
        id: hoverHandler

        target: slider.handle
    }
}
