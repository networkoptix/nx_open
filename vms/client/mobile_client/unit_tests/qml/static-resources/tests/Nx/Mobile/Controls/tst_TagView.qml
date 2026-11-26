// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.TagView"

    component: TagView
    {
        width: 300
        model: ["One", "Two", "Three"]
    }

    function tagAt(index)
    {
        const result = findById("repeater").itemAt(index)
        verify(!!result, "Object exists")
        return result
    }

    function test_hasTags()
    {
        compare(control.hasTags, true)
        control.model = []
        compare(control.hasTags, false)
    }

    function test_tagFitsText()
    {
        const tag = tagAt(0)
        const tagText = findById("tagText", tag)
        compare(tagText.truncated, false)
        compare(tagText.width, tagText.implicitWidth)
        compare(tag.width, tagText.width)
    }

    function test_longTagIsElided()
    {
        control.model = ["A very long tag text which does not fit into the available width at all"]
        const tag = tagAt(0)
        const tagText = findById("tagText", tag)
        compare(tagText.truncated, true)
        verify(tag.width <= testCase.width)
    }
}
