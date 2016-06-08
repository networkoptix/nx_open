import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0

Page
{
    id: page

    property bool activePage: StackView.status === StackView.Active
    property int pageStatus: StackView.status
    property bool sideNavigationEnabled: true

    background: Rectangle { color: ColorTheme.windowBackground }

    onSideNavigationEnabledChanged: updateSideNavigation()
    onActivePageChanged:
    {
        if (activePage)
            updateSideNavigation()
    }

    Keys.onPressed:
    {
        if (Utils.keyIsBack(event.key))
        {
            Workflow.popCurrentScreen()
        }
        else if (event.key == Qt.Key_F2)
        {
            if (sideNavigationEnabled)
                sideNavigation.open()
        }
    }

    function updateSideNavigation()
    {
        if (!sideNavigationEnabled)
            sideNavigation.close()

        sideNavigation.enabled = sideNavigationEnabled
    }
}
