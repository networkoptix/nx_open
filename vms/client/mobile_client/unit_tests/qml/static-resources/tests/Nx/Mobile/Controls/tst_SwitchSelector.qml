// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.SwitchSelector"

    component: SwitchSelector { text: "Option" }
    spiedSignals: ["clicked"]

    function test_defaults()
    {
        compare(control.checkState, Qt.Unchecked)
    }

    function test_descriptionText()
    {
        compare(control.descriptionTextItem.visible, false)
        control.descriptionText = "Description"
        compare(control.descriptionTextItem.visible, true)
    }

    function test_clicked()
    {
        mouseClick(control, 20, control.height / 2)
        tryCompare(signalSpy("clicked"), "count", 1)
        tryCompare(control, "checkState", Qt.Unchecked)
    }

    function test_switchClicked()
    {
        verify(!!control.customAreaItem, "Object exists")
        mouseClick(control.customAreaItem)
        tryCompare(signalSpy("clicked"), "count", 1)
        tryCompare(control, "checkState", Qt.Unchecked)
        tryCompare(control.customAreaItem, "checkState", Qt.Unchecked)
    }

    function test_switchKeepsOwnerStateAfterClick()
    {
        verify(!!control.customAreaItem, "Object exists")
        mouseClick(control.customAreaItem)
        tryCompare(signalSpy("clicked"), "count", 1)

        control.checkState = Qt.Checked
        compare(control.customAreaItem.checkState, Qt.Checked)
    }
}
