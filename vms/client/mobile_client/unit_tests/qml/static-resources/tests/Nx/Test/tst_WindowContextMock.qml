// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Test

TestCase
{
    id: test

    name: "Nx.Test.WindowContextMock"

    readonly property string kWindowContextType: "nx::vms::client::mobile::WindowContext"

    function init()
    {
        failOnWarning(/.?/)
    }

    /** Fails for a mock member which WindowContext does not have. */
    function test_matchesWindowContext()
    {
        compare(TestUtils.mockMismatches(windowContext, kWindowContextType).join("\n"), "")
    }

    function test_mismatchIsFound_data()
    {
        return [
            {tag: "unknown property", component: unknownProperty, member: "unknown"},
            {
                tag: "unknown property of nested object",
                component: unknownNestedProperty,
                member: "ui.measurements.unknown"
            },
            {
                tag: "another property type",
                component: anotherPropertyType,
                member: "ui.measurements.androidKeyboardHeight"
            },
            {
                tag: "value instead of object",
                component: valueInsteadOfObject,
                member: "ui"
            },
            {
                tag: "unknown function",
                component: unknownFunction,
                member: "ui.windowHelpers.unknown"
            },
            {
                tag: "another number of parameters",
                component: anotherParameterCount,
                member: "ui.windowHelpers.enterFullscreen"
            },
            {tag: "unknown signal", component: unknownSignal, member: "unknown"},
            {
                tag: "call signal without function",
                component: callSignalWithoutFunction,
                member: "ui.windowHelpers.enterFullscreenCalled"
            },
            {
                tag: "reset of nested object",
                component: nestedReset,
                member: "ui.reset"
            }
        ]
    }

    function test_mismatchIsFound(data)
    {
        const mock = createTemporaryObject(data.component, test)
        verify(!!mock, "Component exists")

        const mismatches = TestUtils.mockMismatches(mock, kWindowContextType)
        compare(mismatches.length, 1)
        verify(mismatches[0].startsWith(data.member + ":"), mismatches[0])
    }

    function test_unknownClassIsReported()
    {
        const mismatches = TestUtils.mockMismatches(windowContext, "NoSuchClass")
        compare(mismatches.length, 1)
        verify(mismatches[0].startsWith("NoSuchClass "), mismatches[0])
    }

    component Ui: QtObject
    {
        property QtObject measurements
        property QtObject windowHelpers
    }

    Component
    {
        id: unknownProperty

        QtObject
        {
            property int unknown
        }
    }

    Component
    {
        id: unknownNestedProperty

        QtObject
        {
            readonly property QtObject ui: Ui
            {
                measurements: QtObject
                {
                    property int unknown
                }
            }
        }
    }

    Component
    {
        id: anotherPropertyType

        QtObject
        {
            readonly property QtObject ui: Ui
            {
                measurements: QtObject
                {
                    property string androidKeyboardHeight
                }
            }
        }
    }

    Component
    {
        id: valueInsteadOfObject

        QtObject
        {
            property int ui
        }
    }

    Component
    {
        id: unknownFunction

        QtObject
        {
            readonly property QtObject ui: Ui
            {
                windowHelpers: QtObject
                {
                    function unknown() {}
                }
            }
        }
    }

    Component
    {
        id: anotherParameterCount

        QtObject
        {
            readonly property QtObject ui: Ui
            {
                windowHelpers: QtObject
                {
                    function enterFullscreen(extra) {}
                }
            }
        }
    }

    Component
    {
        id: unknownSignal

        QtObject
        {
            signal unknown()
        }
    }

    Component
    {
        id: callSignalWithoutFunction

        QtObject
        {
            readonly property QtObject ui: Ui
            {
                windowHelpers: QtObject
                {
                    signal enterFullscreenCalled()
                }
            }
        }
    }

    Component
    {
        id: nestedReset

        QtObject
        {
            readonly property QtObject ui: QtObject
            {
                function reset() {}
            }
        }
    }
}
