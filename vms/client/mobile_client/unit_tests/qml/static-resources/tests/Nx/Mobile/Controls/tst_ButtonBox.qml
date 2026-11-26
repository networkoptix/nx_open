// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.ButtonBox"

    component: ButtonBox
    {
        model: [
            {id: "cancel", type: Button.Type.Interface, text: "Cancel"},
            {id: "ok", type: Button.Type.Brand, text: "OK"}
        ]
    }

    spiedSignals: ["clicked"]

    function test_buttonById()
    {
        compare(control.buttonById("cancel").text, "Cancel")
        compare(control.buttonById("ok").text, "OK")
        compare(control.buttonById("missing"), null)
    }

    function test_clicked_data()
    {
        return [
            {tag: "cancel", buttonId: "cancel"},
            {tag: "ok", buttonId: "ok"}
        ]
    }

    function test_clicked(data)
    {
        mouseClick(control.buttonById(data.buttonId))
        tryCompare(signalSpy("clicked"), "count", 1)
        compare(signalSpy("clicked").signalArguments[0][0], data.buttonId)
    }

    function test_buttonsShareWidth()
    {
        const cancelButton = control.buttonById("cancel")
        const okButton = control.buttonById("ok")
        compare(control.width, testCase.width)
        compare(cancelButton.width, okButton.width)
        compare(cancelButton.width + control.spacing + okButton.width, control.width)
    }

    function test_singleButtonTakesWholeWidth()
    {
        control.model = [{id: "close", type: Button.Type.Interface, text: "Close"}]
        compare(control.buttonById("cancel"), null)
        compare(control.buttonById("close").width, control.width)
    }

    function test_spacing()
    {
        compare(control.spacing, 12)
        control.model = []
        compare(control.spacing, 0)
    }
}
