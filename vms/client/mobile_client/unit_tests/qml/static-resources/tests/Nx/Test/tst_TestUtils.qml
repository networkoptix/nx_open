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

    function test_findById_ownIdBeforeAssignedObject()
    {
        const holder = createTemporaryObject(holderComponent, test)
        verify(TestUtils.findById(holder, "duplicate") === holder.ownDuplicate)
        verify(TestUtils.findById(holder, "unique") === holder.slot.unique)
    }

    function test_findById_baseTypeIdBeforeAssignedObject()
    {
        const holder = createTemporaryObject(derivedHolderComponent, test)
        verify(TestUtils.findById(holder, "duplicate") === holder.ownDuplicate)
        verify(TestUtils.findById(holder, "unique") === holder.slot.unique)
    }

    function test_findById_idWithNameOfProperty()
    {
        const shadowing = createTemporaryObject(shadowingComponent, test)
        verify(shadowing.shadowed !== shadowing.shadowedById)
        verify(TestUtils.findById(shadowing, "shadowed") === shadowing.shadowedById)

        shadowing.shadowed = null
        verify(TestUtils.findById(shadowing, "shadowed") === shadowing.shadowedById)
    }

    function test_findById_idWithNameOfBaseTypeProperty()
    {
        const shadowing = createTemporaryObject(derivedShadowingComponent, test)
        verify(TestUtils.findById(shadowing, "shadowed") === shadowing.shadowedById)
    }

    function test_findById_ownIdBeforeIdOfComponent()
    {
        const outer = createTemporaryObject(outerWithIdsComponent, test)
        verify(outer.componentDuplicate !== outer.duplicate)
        verify(TestUtils.findById(outer, "duplicate") === outer.duplicate)
    }

    function test_findById_idOfComponentBeforeNestedId()
    {
        const outer = createTemporaryObject(outerWithIdsComponent, test)
        verify(outer.componentUnique !== outer.nested.unique)
        verify(TestUtils.findById(outer, "unique") === outer.componentUnique)
    }

    function test_findById_nestedIdWithNameOfProperty_data()
    {
        return [
            {tag: "object with another id", component: hidingByObjectWithIdComponent},
            {tag: "object without id", component: hidingByObjectWithoutIdComponent}
        ]
    }

    function test_findById_nestedIdWithNameOfProperty(data)
    {
        const hiding = createTemporaryObject(data.component, test)
        verify(hiding.unique && hiding.unique !== hiding.nested.unique)
        verify(TestUtils.findById(hiding, "unique") === hiding.nested.unique)
    }

    function test_findById_propertyIsNotId()
    {
        const shadowing = createTemporaryObject(shadowingComponent, test)
        verify(shadowing.shadowedById)
        verify(TestUtils.findById(shadowing, "shadowedById") === null)
    }

    function test_findById_unknownId()
    {
        const outer = createTemporaryObject(outerComponent, test)
        verify(TestUtils.findById(outer, "unknown") === null)
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

    // An object assigned from outside is created after the own objects of the type.
    component Holder: Item
    {
        property Item slot
        readonly property alias ownDuplicate: duplicate

        Item { id: duplicate }
    }

    component DerivedHolder: Holder {}

    component Shadowing: Item
    {
        property Item shadowed: other
        readonly property alias shadowedById: shadowed

        Item { id: shadowed }
        Item { id: other }
    }

    component DerivedShadowing: Shadowing {}

    // The property has the name of the id which only the nested component has.
    component HidingByObjectWithId: Item
    {
        property Item unique: other
        readonly property alias nested: nested

        Item { id: other }
        Nested { id: nested }
    }

    component HidingByObjectWithoutId: Item
    {
        property Item unique: Item {}
        readonly property alias nested: nested

        Nested { id: nested }
    }

    Component
    {
        id: outerComponent

        Outer {}
    }

    // The ids are declared where the object is instantiated, not in its type.
    Component
    {
        id: outerWithIdsComponent

        Outer
        {
            readonly property alias componentDuplicate: duplicate
            readonly property alias componentUnique: unique

            Item { id: duplicate }
            Item { id: unique }
        }
    }

    Component
    {
        id: holderComponent

        Holder { slot: Nested {} }
    }

    Component
    {
        id: derivedHolderComponent

        DerivedHolder { slot: Nested {} }
    }

    Component
    {
        id: shadowingComponent

        Shadowing {}
    }

    Component
    {
        id: derivedShadowingComponent

        DerivedShadowing {}
    }

    Component
    {
        id: hidingByObjectWithIdComponent

        HidingByObjectWithId {}
    }

    Component
    {
        id: hidingByObjectWithoutIdComponent

        HidingByObjectWithoutId {}
    }
}
