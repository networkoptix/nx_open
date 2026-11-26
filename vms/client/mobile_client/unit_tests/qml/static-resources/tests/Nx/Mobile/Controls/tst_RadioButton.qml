// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.RadioButton"

    component: RadioButton { text: "Option" }
    spiedSignals: ["clicked"]

    function test_text()
    {
        const label = findById("label")
        compare(label.visible, true)

        control.text = ""
        compare(label.visible, false)
    }

    function test_clicked()
    {
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
        tryCompare(control, "checked", true)
    }

    function test_clickDoesNotUncheck()
    {
        control.checked = true
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
        tryCompare(control, "checked", true)
    }

    function test_icon()
    {
        const image = findById("image")
        compare(image.visible, false)

        const widthWithoutIcon = control.implicitWidth
        control.icon.source = "image://skin/24x24/Outline/close.svg"
        compare(image.visible, true)
        tryCompare(image, "status", Image.Ready)
        tryCompare(control, "implicitWidth",
            widthWithoutIcon + image.implicitWidth + control.spacing)
    }
}
