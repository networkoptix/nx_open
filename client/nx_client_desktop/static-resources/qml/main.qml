import QtQuick 2.6
import Qt.labs.templates 1.0

import Nx 1.0
import Nx.Layout 1.0
import nx.client.desktop 1.0

Control
{
    id: mainWindowRoot

    background: Rectangle { color: ColorTheme.window }

    contentItem: LayoutViewer
    {
        anchors.fill: parent
        layout: workbench.currentLayout.resource
    }
}
