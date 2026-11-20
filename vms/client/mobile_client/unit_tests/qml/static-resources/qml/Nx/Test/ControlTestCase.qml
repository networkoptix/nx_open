// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Test

/** Creates `component` as `control` before every test function. */
TestCase
{
    id: test

    property Component component
    property QtObject control
    property list<string> spiedSignals

    // Required for the mouse events, since they reach only a visible item of the shown window.
    when: windowShown
    visible: true
    width: 400
    height: 400

    function init()
    {
        initBase()
    }

    function test_implicitSize()
    {
        verify(control.implicitWidth > 0)
        verify(control.implicitHeight > 0)

        // The control must fit into the test window, otherwise mouse events may not reach it.
        verify(control.implicitWidth <= test.width)
        verify(control.implicitHeight <= test.height)
        verify(control.width <= test.width)
        verify(control.height <= test.height)
    }

    /** Must be called from an overridden init(), since QML cannot call the base one. */
    function initBase()
    {
        failOnWarning(/.?/)
        windowContext.reset()
        control = createTemporaryObject(component, test)
        signals.reset()
    }

    function signalSpy(name)
    {
        const result = signals.itemAt(spiedSignals.indexOf(name))
        verify(result && result.valid, `No such signal "${name}"`)
        return result
    }

    function findById(id, root = control)
    {
        const result = TestUtils.findById(root, id)
        verify(result, `No such object "${id}"`)
        return result
    }

    Repeater
    {
        id: signals

        function reset()
        {
            for (let i = 0; i < count; i++)
                itemAt(i).clear()
        }

        model: test.spiedSignals

        delegate: SignalSpy
        {
            target: test.control
            signalName: modelData
        }
    }
}
