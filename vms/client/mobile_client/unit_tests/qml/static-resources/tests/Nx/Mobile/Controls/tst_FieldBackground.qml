// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.FieldBackground"

    component: FieldBackground
    {
        owner: fieldOwner
        width: fieldOwner.width
        height: fieldOwner.height
    }

    function cleanup()
    {
        fieldOwner.text = ""
        fieldOwner.errorText = ""
        fieldOwner.focus = false
    }

    /** The background has no implicit size: its owner sets the size. */
    function test_implicitSize()
    {
        compare(control.implicitWidth, 0)
        compare(control.implicitHeight, 0)
    }

    function test_defaultMode()
    {
        compare(control.mode, FieldBackground.Mode.Dark)
    }

    function test_modeColor_data()
    {
        return [
            {tag: "light", mode: FieldBackground.Mode.Light, color: ColorTheme.colors.dark11},
            {tag: "middle", mode: FieldBackground.Mode.Middle, color: ColorTheme.colors.dark9},
            {tag: "dark", mode: FieldBackground.Mode.Dark, color: ColorTheme.colors.dark7}
        ]
    }

    function test_modeColor(data)
    {
        control.mode = data.mode
        verify(Qt.colorEqual(control.color, data.color))
    }

    function test_labelIsCompactWithText()
    {
        const label = findById("label")
        compare(control.compactLabelMode, false)
        compare(label.y, 18)
        compare(label.font.pixelSize, 16)

        fieldOwner.text = "Text"
        compare(control.compactLabelMode, true)
        compare(label.y, 8)
        compare(label.font.pixelSize, 12)
    }

    function test_labelIsCompactWithFocus()
    {
        fieldOwner.forceActiveFocus()
        verify(fieldOwner.activeFocus)
        compare(control.compactLabelMode, true)

        fieldOwner.focus = false
        compare(control.compactLabelMode, false)
    }

    function test_borderColor()
    {
        verify(Qt.colorEqual(control.border.color, ColorTheme.colors.dark7))

        fieldOwner.forceActiveFocus()
        verify(fieldOwner.activeFocus)
        verify(Qt.colorEqual(control.border.color, ColorTheme.colors.brand_core))

        fieldOwner.errorText = "Error"
        verify(Qt.colorEqual(control.border.color, ColorTheme.colors.attention.red))
    }

    function test_supportText()
    {
        const bottomText = findById("bottomText")
        compare(bottomText.visible, false)
        compare(control.bottomTextHeight, 0)

        control.supportText = "Support"
        compare(bottomText.text, "Support")
        compare(bottomText.visible, true)
        verify(Qt.colorEqual(bottomText.color, ColorTheme.colors.light10))
        verify(control.bottomTextHeight > 0)
        compare(control.bottomTextHeight, bottomText.height)
    }

    function test_errorTextReplacesSupportText()
    {
        const bottomText = findById("bottomText")
        control.supportText = "Support"
        control.errorText = "Error"
        compare(bottomText.text, "Error")
        verify(Qt.colorEqual(bottomText.color, ColorTheme.colors.attention.red))

        control.errorText = ""
        compare(bottomText.text, "Support")
    }

    function test_disabledOpacity()
    {
        compare(control.opacity, 1)
        control.enabled = false
        compare(control.opacity, 0.3)
    }

    // Stands for the control which owns the background.
    Item
    {
        id: fieldOwner

        property string text
        property string errorText
        property real leftPadding: 12
        property real rightPadding: 12

        width: 268
        height: 56
    }
}
