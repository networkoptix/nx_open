// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

ScrollView
{
    id: control

    property alias textArea: textArea
    property alias text: textArea.text
    property alias wrapMode: textArea.wrapMode
    property alias readOnly: textArea.readOnly
    property alias cursorPosition: textArea.cursorPosition

    signal textEdited()
    signal editingFinished()

    implicitWidth: 200
    implicitHeight: 64
    baselineOffset: textArea.baselineOffset

    TextArea
    {
        id: textArea

        leftPadding: 8
        rightPadding: control.ScrollBar.vertical.visible ? 12 : 8
        bottomPadding: control.ScrollBar.horizontal.visible ? 12 : 4

        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight

        wrapMode: Text.Wrap

        onTextEdited: control.textEdited()
        onEditingFinished: control.editingFinished()
    }
}
