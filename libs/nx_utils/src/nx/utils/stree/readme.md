# Simple decision tree {#nx_utils_stree}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## Introduction

The tree is a set of conditions. A condition checks value of an attribute and based on that value
passes control to another condition or sets an output attribute.
A query to the tree has a set of input attributes and produces a set of output attributes.
The tree is defined by an xml document.
Example:

    <?xml version="1.0" encoding="utf-8"?>
    <condition resName="colorName">
        <sequence value="black">
            <set resName="r" resValue="0"/>
            <set resName="g" resValue="0"/>
            <set resName="b" resValue="0"/>
        </sequence>
        <sequence value="white">
            <set resName="r" resValue="255"/>
            <set resName="g" resValue="255"/>
            <set resName="b" resValue="255"/>
        </sequence>
    </condition>

## Tree traversal

When we query the tree using input attributes `(colorName:black)` then the following output
attributes will be produced: `(r:0; g:0; b:0)`.
The tree was traversed in the following order:
- top level node `<condition resName="colorName">` receives control
- it tests the value attribute `colorName` from the input attribute set `(colorName:black)`
- value `black` matches that of its child node `<sequence value="black">`, so it receives the control
- `<sequence value="black">` just gives control to each its child:
- `<set resName="r" resValue="0"/>` receives control and add `r` attribute with value `0` to
the output attributes set
- then, nodes `<set resName="g" resValue="0"/>` and `<set resName="b" resValue="0"/>` do similar
- the node `<sequence value="white">` is not invoked because its value `white` is not matched

So, a tree is a combination of nodes of three types shown above.
Any number of attributes may be present in a tree.

## Supported attribute value match algorithms

`condition` node may have an optional `matchType` attribute. E.g.,

    <condition resName="http.request.path" matchType="wildcard">
      <set value="/foo/*" resName="authenticated" resValue="true"/>

It means that values child nodes of `<condition>` will be tested as wildcard expressions.
If `matchType` is not specified, then `equal` match type is used.
Supported match types are specified by nx::utils::stree::MatchType.

## Code example

The example found above is represented in the code as follows:

    static constexpr char kTreeDoc[] = <Xml document defined above>;

    // Loading tree into memory.
    auto treeRoot = nx::utils::stree::StreeManager::loadStree(kTreeDoc);

    // Querying the tree.
    nx::utils::stree::AttributeDictionary inAttrs{{"colorName", "black"}};
    nx::utils::stree::AttributeDictionary outAttrs;
    treeRoot->get(inAttrs, &outAttrs);

    // Testing the result.
    assert(outAttrs.get<int>("r") == 0);
    assert(outAttrs.get<int>("g") == 0);
    assert(outAttrs.get<int>("b") == 0);
