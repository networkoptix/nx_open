// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import "private"

Group
{
    property string name: ""
    childrenItem: row

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    contentItem: Row
    {
        id: row
        spacing: 8
    }
}
