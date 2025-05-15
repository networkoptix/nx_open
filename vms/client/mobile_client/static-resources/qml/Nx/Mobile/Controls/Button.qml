// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

import Nx.Core
import Nx.Core.Controls

T.AbstractButton
{
    id: control

    enum Type
    {
        Brand,
        Interface,
        LightInterface

        // TODO: #ynikitenkov Add other types of button.
        // SemiTransparent,
        // Transparent
    }

    property bool showBusyIndicator: false
    property int type: Button.Type.Interface

    implicitHeight: 44
    implicitWidth: textItem.width + control.leftPadding + control.rightPadding

    padding: 10
    leftPadding: 16
    rightPadding: 16

    background: Rectangle
    {
        radius: 6
        color: d.parameters[control.type].colors[control.state]
        border.color:
        {
            const params = d.parameters[control.type]
            const borderColors = params.borderColors
            return (borderColors && borderColors[control.state]) ?? params.colors[control.state]
        }
    }

    contentItem: Item
    {
        Text
        {
            id: textItem

            visible: !control.showBusyIndicator

            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            width: control.width - control.leftPadding - control.rightPadding

            text: control.text
            wrapMode: Text.Wrap

            font.pixelSize: 16
            font.weight: 500
            color: d.parameters[control.type].textColors[control.state]
        }

        SpinnerBusyIndicator
        {
            visible: control.showBusyIndicator

            anchors.centerIn: parent
            radius: 16
            running: true
        }
    }

    state:
    {
        if (!control.enabled)
            return "disabled"

        if (control.pressed)
        {
            return control.checked
                ? "pressed_checked"
                : "pressed_unchecked"
        }

        return control.checked
            ? "default_checked"
            : "default_unchecked"
    }

    NxObject
    {
        id: d

        readonly property var parameters: {

            let result = {}
            result[Button.Type.Brand] = {
                colors: {
                    default_unchecked: ColorTheme.colors.brand_core,
                    pressed_unchecked: ColorTheme.colors.brand_l1,
                    default_checked: ColorTheme.colors.brand_core,
                    pressed_checked: ColorTheme.colors.brand_l1,
                    disabled: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.3)
                },
                textColors: {
                    default_unchecked: ColorTheme.colors.dark1,
                    pressed_unchecked: ColorTheme.colors.dark1,
                    default_checked: ColorTheme.colors.dark1,
                    pressed_checked: ColorTheme.colors.dark1,
                    disabled: ColorTheme.transparent(ColorTheme.colors.dark1, 0.3)
                }
            }

            result[Button.Type.Interface] = {
                colors: {
                    default_unchecked: ColorTheme.colors.dark8,
                    pressed_unchecked: ColorTheme.colors.dark10,
                    default_checked: ColorTheme.colors.dark10,
                    pressed_checked: ColorTheme.colors.dark12,
                    disabled: ColorTheme.transparent(ColorTheme.colors.dark8, 0.3)
                },
                borderColors: {
                    default_checked: ColorTheme.colors.dark14,
                    pressed_checked: ColorTheme.colors.dark14
                },
                textColors: {
                    default_unchecked: ColorTheme.colors.light10,
                    pressed_unchecked: ColorTheme.colors.light10,
                    default_checked: ColorTheme.colors.light4,
                    pressed_checked: ColorTheme.colors.light4,
                    disabled: ColorTheme.transparent(ColorTheme.colors.light10, 0.3)
                }
            }

            result[Button.Type.LightInterface] = {
                colors: {
                    default_unchecked: ColorTheme.colors.dark12,
                    pressed_unchecked: ColorTheme.colors.dark14,
                    default_checked: ColorTheme.colors.dark14,
                    pressed_checked: ColorTheme.colors.dark16,
                    disabled: ColorTheme.transparent(ColorTheme.colors.dark12, 0.3)
                },
                borderColors: {
                    default_checked: ColorTheme.colors.dark18,
                    pressed_checked: ColorTheme.colors.dark18
                },
                textColors: {
                    default_unchecked: ColorTheme.colors.light10,
                    pressed_unchecked: ColorTheme.colors.light10,
                    default_checked: ColorTheme.colors.light4,
                    pressed_checked: ColorTheme.colors.light4,
                    disabled: ColorTheme.transparent(ColorTheme.colors.light10, 0.3)
                }
            }
            return result;
        }
    }
}
