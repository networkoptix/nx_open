// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

FocusScope
{
    id: control

    required property Analytics.StateView taxonomy
    property bool isEditing: false
    focus: isEditing
    anchors.fill: parent

    // Never pass key presses to parents while editing.
    Keys.onPressed: (event) => event.accepted = isEditing

    function edit()
    {
        console.log("Edit started")
        editor.value = model.display
        isEditing = true
        editor.setFocus()
    }

    function commit()
    {
        console.log("Commit")
        model.display = editor.value
        isEditing = false
    }

    function revert()
    {
        console.log("Revert")
        editor.value = model.display
        isEditing = false
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: control.edit()
        visible: !control.isEditing
    }

    BasicTableCellDelegate
    {
        x: 8
        y: 6

        text: model.display || qsTr("ANY")
        color: model.display ? ColorTheme.light : ColorTheme.colors.dark17
        visible: !control.isEditing
    }

    LookupListElementEditor
    {
        id: editor

        visible: control.isEditing
        anchors.fill: parent
        focus: true
        clip: true
        value: model.display

        taxonomy: control.taxonomy
        objectTypeId: model.objectTypeId
        attributeName: model.attributeName

        onEditingFinished:
        {
            console.log("Editing Finished")
            commit()
        }

        Keys.onPressed: (event) =>
        {
            event.accepted = true
            switch (event.key)
            {
                case Qt.Key_Enter:
                case Qt.Key_Return:
                    control.commit()
                    break

                case Qt.Key_Escape:
                    control.revert()
                    break

                default:
                    event.accepted = false
                    break
            }
        }

        Keys.onShortcutOverride: (event) =>
        {
            switch (event.key)
            {
                case Qt.Key_Enter:
                case Qt.Key_Return:
                case Qt.Key_Escape:
                    event.accepted = true
                    break
            }
        }
    }
}
