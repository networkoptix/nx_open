import QtQuick 2.0
import Nx.Controls 1.0

import "private"

GroupBox
{
    id: control

    property string name: ""
    property alias caption: control.title
    property string description: ""

    property alias childrenItem: control.contentItem

    width: parent.width
    contentItem: AlignedColumn {}
}
