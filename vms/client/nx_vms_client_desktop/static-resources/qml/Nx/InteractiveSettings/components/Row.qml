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
