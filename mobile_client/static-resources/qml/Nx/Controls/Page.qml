import QtQuick 2.0
import Nx.Controls 1.0

PageBase
{
    id: page

    property alias title: toolBar.title
    property alias leftButtonIcon: toolBar.leftButtonIcon
    property alias titleControls: toolBar.controls
    property alias warningText: warningPanel.text
    property alias warningVisible: warningPanel.opened

    signal leftButtonClicked()

    header: Item
    {
        implicitWidth: parent.width
        implicitHeight: toolBar.height + warningPanel.height

        ToolBar
        {
            id: toolBar
            leftButtonIcon: "/images/arrow_back.png"
            onLeftButtonClicked: page.leftButtonClicked()
        }

        WarningPanel
        {
            id: warningPanel
            anchors.top: toolBar.bottom
        }
    }
}
