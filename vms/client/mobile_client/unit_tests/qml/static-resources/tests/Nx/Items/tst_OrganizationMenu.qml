// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Items
import Nx.Test

ControlTestCase
{
    id: test

    name: "Nx.Items.OrganizationMenu"

    component: OrganizationMenu { name: "Organization" }
    spiedSignals: ["structureClicked", "logInClicked"]

    function test_implicitSize()
    {
        skip("Not a visual control, its content is shown in a popup")
    }

    function test_action_data()
    {
        return [
            {tag: "menu/structure", expanded: true, itemId: "structureMenuItem",
                signalName: "structureClicked"},
            {tag: "menu/logIn", expanded: true, itemId: "logInMenuItem",
                signalName: "logInClicked"},
            {tag: "sheet/structure", expanded: false, itemId: "structureButton",
                signalName: "structureClicked"},
            {tag: "sheet/logIn", expanded: false, itemId: "logInButton",
                signalName: "logInClicked"},
            {tag: "sheet/cancel", expanded: false, itemId: "cancelButton", signalName: ""},
        ]
    }

    function test_action(data)
    {
        openMenu(data.expanded)
        compare(control.anchored, data.expanded)

        mouseClick(findById(data.itemId))
        tryCompare(control, "opened", false)

        for (const name of spiedSignals)
            compare(signalSpy(name).count, name === data.signalName ? 1 : 0, name)

        compare(control.anchored, false)
    }

    function test_reopenInAnotherLayout()
    {
        openMenu(/*expanded*/ false)
        compare(control.anchored, false)
        closeMenu()

        openMenu(/*expanded*/ true)
        compare(control.anchored, true)
        closeMenu()

        openMenu(/*expanded*/ false)
        compare(control.anchored, false)
        closeMenu()
    }

    function openMenu(expanded)
    {
        control.expandedLayout = expanded
        control.open(menuAnchor)

        const popup = findById("loader").item
        tryCompare(popup, "opened", true)
    }

    function closeMenu()
    {
        control.close()
        tryCompare(control, "opened", false)
    }

    // The menu is shown to the left of the anchor and must fit into the test window.
    Item
    {
        id: menuAnchor

        x: test.width - 10
        y: 10
    }
}
