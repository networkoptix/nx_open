import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0
import Nx.Controls 1.0

Control
{
    id: control

    property bool active: false

    signal clicked()

    implicitWidth: control.parent ? control.parent.width : 200
    implicitHeight: 48

    font.pixelSize: 16
    font.weight: Font.DemiBold

    background: Item
    {
        Rectangle
        {
            anchors.fill: parent
            visible: control.activeFocus || control.active
            color: ColorTheme.base9

            Rectangle
            {
                height: parent.height
                width: control.active ? 3 : 1
                color: ColorTheme.brand_main
            }
        }

        MaterialEffect
        {
            anchors.fill: parent
            anchors.leftMargin: control.active ? 3 : 1
            mouseArea: mouseArea
            clip: true
            rippleSize: 160
        }
    }

    MouseArea
    {
        id: mouseArea
        parent: control
        anchors.fill: control
        onClicked:
        {
            sideNavigation.close()
            control.clicked()
        }
    }

    Keys.onEnterPressed: clicked()
    Keys.onReturnPressed: clicked()
}

