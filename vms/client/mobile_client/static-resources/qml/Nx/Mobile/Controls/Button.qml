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

    property real textIndent: 0 //< Additional text indent from the left.
    property alias textHorizontalAlignment: textItem.horizontalAlignment

    readonly property var parameters: d.parameters[control.type]

    property color backgroundColor: parameters.colors[state]
    property color foregroundColor: parameters.textColors[state]
    property real radius: 6

    property color borderColor: parameters.borderColors
        ? (parameters.borderColors[state] ?? "transparent")
        : "transparent"

    implicitHeight: 44
    implicitWidth: (contentItem?.implicitWidth ?? 0) + control.leftPadding + control.rightPadding

    focusPolicy: Qt.TabFocus
    padding: 10
    leftPadding: 16
    rightPadding: 16
    spacing: 4

    font.pixelSize: 16
    font.weight: 500

    icon.width: 20
    icon.height: 20

    background: Rectangle
    {
        color: control.backgroundColor
        radius: control.radius
        border.color: control.borderColor
    }

    contentItem: Item
    {
        id: content

        readonly property bool hasIcon: !!imageItem.sourcePath
        readonly property bool hasText: !!textItem.text

        implicitWidth:
        {
            let result = content.hasIcon ? imageItem.implicitWidth : 0
            if (content.hasText)
                result += textItem.implicitWidth
            if (content.hasText && content.hasIcon)
                result += control.spacing
            return result
        }

        implicitHeight:
        {
            let result = content.hasIcon ? imageItem.implicitHeight : 0
            if (content.hasText)
                result = Math.max(result, textItem.implicitHeight)
            return result
        }

        Row
        {
            anchors.horizontalCenter: content.horizontalCenter
            height: content.height
            spacing: control.spacing
            visible: !control.showBusyIndicator

            ColoredImage
            {
                id: imageItem

                name: control.icon.name
                sourcePath: control.icon.source
                sourceSize: Qt.size(control.icon.width, control.icon.height)
                primaryColor: control.foregroundColor
                visible: content.hasIcon

                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: (parent.height - height) % 2
            }

            Text
            {
                id: textItem

                visible: content.hasText

                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: (parent.height - height) % 2

                leftPadding: control.textIndent

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                width: content.hasIcon
                    ? control.availableWidth - imageItem.width - control.spacing
                    : control.availableWidth

                text: control.text
                wrapMode: Text.Wrap

                font: control.font
                color: control.foregroundColor
            }
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

        if (control.down)
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
            return result
        }
    }
}
