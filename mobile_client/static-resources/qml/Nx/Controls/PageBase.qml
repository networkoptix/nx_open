import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0

Page
{
    id: page

    property bool activePage: StackView.status === StackView.Active
    property int pageStatus: StackView.status

    background: Rectangle { color: ColorTheme.windowBackground }
}
