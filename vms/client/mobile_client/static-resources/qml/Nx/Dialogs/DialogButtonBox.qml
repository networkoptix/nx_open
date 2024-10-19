// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml 2.2
import QtQuick 2.6
import QtQuick.Layouts 1.15
import QtQuick.Templates 2.12 as T

import Nx.Core 1.0

T.DialogButtonBox
{
    id: control

    signal buttonClicked(string buttonId)

    width: parent.width
    height: separator.height + buttonsRow.height

    property alias buttonsModel: instantiator.model

    function setButtonEnabled(id, value)
    {
        for (let i = 0; i !== buttonsRow.children.length; ++i)
        {
            const child = buttonsRow.children[i]
            if (child.objectName !== id)
                continue

            child.enabled = value
            return
        }
    }

    contentItem: ColumnLayout
    {
        spacing: 0
        width: control.width

        DialogSeparator
        {
            id: separator
            width: parent.width
        }

        RowLayout
        {
            id: buttonsRow
            width: parent.width
        }
    }

    Component
    {
        id: buttonComponent

        DialogButton
        {
            parent: buttonsRow
            objectName: modelData.id ? modelData.id : ""
            accented: !!modelData.accented
            text: modelData.text ? modelData.text : d._standardButtonName(modelData)
            onClicked: control.buttonClicked(objectName)
            Layout.fillWidth: true
        }
    }

    delegate: buttonComponent

    Instantiator
    {
        id: instantiator

        model: [ "OK" ]
        delegate: buttonComponent
    }

    NxObject
    {
        id: d

        readonly property var _standardButtonNames:
        {
            "OK": qsTr("OK"),
            "CANCEL": qsTr("Cancel"),
            "CLOSE": qsTr("Close"),
            "YES": qsTr("Yes"),
            "NO": qsTr("No"),
            "ABORT": qsTr("Abort"),
            "RETRY": qsTr("Retry"),
            "CONNECT": qsTr("Connect")
        }

        function _standardButtonName(modelData)
        {
            if (!modelData)
                return ""

            var id = modelData.id ? modelData.id : modelData
            var text = _standardButtonNames[id]
            return text ? text : id
        }
    }
}
