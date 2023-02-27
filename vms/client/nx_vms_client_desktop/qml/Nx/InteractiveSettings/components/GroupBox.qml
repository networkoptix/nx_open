// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQml 2.14

import Nx.Controls 1.0

import "private"

Group
{
    id: control

    property string name: ""
    property alias caption: groupBox.title
    property string description: ""

    property bool isGroup: true
    property bool fillWidth: true

    childrenItem: groupBox.contentItem

    implicitWidth: groupBox.implicitWidth
    implicitHeight: groupBox.implicitHeight

    contentItem: GroupBox
    {
        id: groupBox

        contentItem: LabeledColumnLayout
        {
            id: column
            layoutControl: control
        }
    }
}
