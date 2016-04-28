import Nx.Controls 1.0

PageBase
{
    id: page

    property alias title: toolBar.title
    property alias leftButtonIcon: toolBar.leftButtonIcon
    property alias titleControls: toolBar.controls

    signal leftButtonClicked()

    header: ToolBar
    {
        id: toolBar
        onLeftButtonClicked: page.leftButtonClicked()
    }
}
