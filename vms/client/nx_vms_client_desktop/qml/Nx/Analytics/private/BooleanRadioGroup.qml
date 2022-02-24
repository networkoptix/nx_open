// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

RadioGroup
{
    id: booleanEditor

    textRole: "text"
    valueRole: "value"

    model: [
        {"text": qsTr("Yes"), "value": true},
        {"text": qsTr("No"), "value": false}]
}
