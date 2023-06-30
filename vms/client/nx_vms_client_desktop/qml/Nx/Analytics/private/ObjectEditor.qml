// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import nx.vms.client.core.analytics 1.0 as Analytics

Column
{
    id: control

    property Analytics.Attribute attribute
    property var loggingCategory

    property alias selectedValue: mainEditor.selectedValue
    readonly property alias selectedText: mainEditor.selectedText
    readonly property alias nestedSelectedValues: nestedAttributes.selectedAttributeValues

    property alias container: nestedAttributes.parent

    function setSelectedAttributeValues(nameValueTree)
    {
        nestedAttributes.selectedAttributeValues = nameValueTree
    }

    spacing: 2

    RadioGroup
    {
        id: mainEditor

        width: control.width

        textRole: "text"
        valueRole: "value"

        model: [
            {"text": qsTr("Present"), "value": true},
            {"text": qsTr("Absent"), "value": false}]
    }

    ObjectAttributes
    {
        id: nestedAttributes

        width: parent.width
        visible: !!selectedValue
        attributes: (attribute && attribute.attributeSet && selectedValue)
            ? attribute.attributeSet.attributes
            : null

        prefix: attribute ? `${attribute.name} \u2192 ` : ""
        loggingCategory: control.loggingCategory
    }
}
