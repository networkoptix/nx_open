// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Test
import Nx.Ui

TestCase
{
    id: testCase

    name: "Nx.Ui.Workflow"

    // Workflow.js takes the screen stack from the scope of the caller.
    readonly property Item stackView: null

    // Becomes null when the dialog is destroyed.
    property QtObject dialog: null

    when: windowShown
    visible: true
    width: 400
    height: 400

    function init()
    {
        failOnWarning(/.?/)
        mainWindow.reset()
        mainWindow.contentItem = testCase
    }

    function cleanup()
    {
        dialog?.destroy()
    }

    function test_openStandardPopup()
    {
        dialog = Workflow.openStandardPopup("Title", "Message", testCase)
        verify(dialog, "Dialog exists")
        tryCompare(dialog, "opened", true)
        compare(TestUtils.findById(dialog, "title").text, "Title")
        compare(dialog.messages, ["Message"])
    }

    function test_dialogIsDestroyedWhenClosed_data()
    {
        return [
            {tag: "popup", open: () => Workflow.openStandardPopup("Title", "Message", testCase)},
            {
                tag: "dialog",
                open: () => Workflow.openStandardDialog(
                    "Title", "Message", ["OK"], /*disableAutoClose*/ false, testCase)
            },
        ]
    }

    function test_dialogIsDestroyedWhenClosed(data)
    {
        dialog = data.open()
        verify(dialog, "Dialog exists")
        tryCompare(dialog, "opened", true)

        dialog.close()
        tryCompare(testCase, "dialog", null)
    }
}
