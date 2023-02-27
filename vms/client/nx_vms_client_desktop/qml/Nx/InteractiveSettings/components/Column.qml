// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import "private"

Group
{
    id: control

    property string name: ""
    childrenItem: column

    property bool fillWidth: true

    contentItem: LabeledColumnLayout
    {
        id: column

        layoutControl: control
    }
}
