// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Test

TestCase
{
    id: test

    name: "Nx.Test.TestUtils"

    function init()
    {
        failOnWarning(/.?/)
    }

    function test_findById_idOfNestedComponent()
    {
        const outer = createTemporaryObject(outerComponent, test)
        verify(TestUtils.findById(outer, "unique") === outer.nested.unique)
    }

    function test_findById_ownIdTakesPrecedence()
    {
        const outer = createTemporaryObject(outerComponent, test)
        verify(TestUtils.findById(outer, "duplicate") === outer.duplicate)
    }

    function test_findById_nestedObjectAsRoot()
    {
        const outer = createTemporaryObject(outerComponent, test)
        verify(TestUtils.findById(outer.nested, "duplicate") === outer.nested.duplicate)
    }

    component Nested: Item
    {
        readonly property alias duplicate: duplicate
        readonly property alias unique: unique

        Item { id: duplicate }
        Item { id: unique }
    }

    component Outer: Item
    {
        readonly property alias duplicate: duplicate
        readonly property alias nested: nested

        Item { id: duplicate }
        Nested { id: nested }
    }

    Component
    {
        id: outerComponent

        Outer {}
    }
}
