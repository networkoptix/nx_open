import QtQuick 2.6
import Nx.Controls 1.0

PageBase
{
    id: page

    property alias title: toolBar.title
    property alias leftButtonIcon: toolBar.leftButtonIcon
    property alias titleControls: toolBar.controls
    property alias warningText: warningPanel.text
    property alias warningVisible: warningPanel.opened
    property alias toolBar: toolBar
    property alias warningPanel: warningPanel
    property alias titleLabelOpactiy: toolBar.titleOpacity

    signal leftButtonClicked()

    clip: true

    header: Item
    {
        width: parent.width

        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        Column
        {
            id: column
            width: parent.width
            topPadding: toolBar.statusBarHeight

            ToolBar
            {
                id: toolBar
                leftButtonIcon: lp("/images/arrow_back.png")
                onLeftButtonClicked: page.leftButtonClicked()
            }

            WarningPanel
            {
                id: warningPanel
            }
        }
    }
}
