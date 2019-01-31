import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx.Controls 1.0 as Nx

ScrollView
{
    id: control

    ScrollBar.vertical: Nx.ScrollBar
    {
        parent: control
        x: control.width - width
        y: control.topPadding
        height: control.availableHeight
        active: control.ScrollBar.vertical.active
    }

    ScrollBar.horizontal: Nx.ScrollBar
    {
        parent: control
        x: control.leftPadding
        y: control.height - height
        width: control.availableWidth
        active: control.ScrollBar.horizontal.active
    }
}
