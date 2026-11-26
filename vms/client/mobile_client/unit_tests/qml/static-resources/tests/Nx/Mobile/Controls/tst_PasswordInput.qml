// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.PasswordInput"

    component: PasswordInput {}
    spiedSignals: ["accepted", "secretIsCleared"]

    function test_hasSecretValueByDefault()
    {
        const textInput = findById("textInput")
        compare(control.hasSecretValue, true)
        compare(control.text, "*************")
        compare(textInput.enabled, false)
        compare(textInput.echoMode, TextInput.Password)
        compare(findById("clearSecretButton").visible, true)
    }

    function test_implicitWidthDoesNotDependOnSecret()
    {
        const textInput = findById("textInput")
        const clearSecretButton = findById("clearSecretButton")
        compare(control.implicitWidth, textInput.implicitWidth)
        compare(control.width, control.implicitWidth)
        compare(textInput.width + findById("row").spacing + clearSecretButton.width, control.width)

        control.resetState(false)
        compare(control.implicitWidth, textInput.implicitWidth)
        compare(textInput.width, control.width)
    }

    function test_explicitWidth()
    {
        control.width = 300
        compare(control.implicitWidth, findById("textInput").implicitWidth)
        tryCompare(findById("row"), "width", 300)
    }

    function test_resetStateWithoutSecret()
    {
        const textInput = findById("textInput")
        control.resetState(false)
        compare(control.hasSecretValue, false)
        compare(control.text, "")
        compare(textInput.enabled, true)
        compare(findById("clearSecretButton").visible, false)
        compare(textInput.width, control.width)
    }

    function test_resetStateWithSecret()
    {
        control.resetState(false)
        control.text = "secret"
        control.resetState(true)
        compare(control.hasSecretValue, true)
        compare(control.text, "*************")
        compare(findById("textInput").enabled, false)
    }

    function test_clearSecretButton()
    {
        const textInput = findById("textInput")
        control.errorText = "Error"
        mouseClick(findById("clearSecretButton"))
        tryCompare(signalSpy("secretIsCleared"), "count", 1)
        tryCompare(control, "hasSecretValue", false)
        tryCompare(control, "text", "")
        tryCompare(control, "errorText", "")
        tryCompare(textInput, "activeFocus", true)
    }

    function test_textIsSelectedOnFocus()
    {
        const textInput = findById("textInput")
        control.resetState(false)
        control.text = "secret"
        textInput.forceActiveFocus()
        verify(textInput.activeFocus)
        compare(textInput.selectedText, "secret")
    }

    function test_maskedModeIsToggledByActionButton()
    {
        const textInput = findById("textInput")
        control.resetState(false)
        control.text = "secret"
        textInput.forceActiveFocus()
        verify(textInput.activeFocus)
        compare(textInput.showActionButton, true)
        compare(textInput.echoMode, TextInput.Password)

        mouseClick(textInput.actionButtonImage)
        tryCompare(textInput, "echoMode", TextInput.Normal)
        tryCompare(control, "text", "secret")

        mouseClick(textInput.actionButtonImage)
        tryCompare(textInput, "echoMode", TextInput.Password)
    }

    function test_resetStateRestoresMaskedMode()
    {
        const textInput = findById("textInput")
        control.resetState(false)
        textInput.maskedMode = false
        compare(textInput.echoMode, TextInput.Normal)

        control.resetState(false)
        compare(textInput.maskedMode, true)
        compare(textInput.echoMode, TextInput.Password)
    }

    function test_accepted()
    {
        const textInput = findById("textInput")
        control.resetState(false)
        control.text = "secret"
        textInput.forceActiveFocus()
        verify(textInput.activeFocus)
        keyClick(Qt.Key_Return)
        tryCompare(signalSpy("accepted"), "count", 1)
    }
}
