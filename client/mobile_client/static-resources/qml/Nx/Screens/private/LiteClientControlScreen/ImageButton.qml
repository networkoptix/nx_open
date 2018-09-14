import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Controls 1.0
import nx.client.core 1.0

Button
{
    id: control

    property alias image: normalImage.source
    property alias checkedImage: checkedImage.source

    property real mouseX: mouseTracker.mouseX
    property real mouseY: mouseTracker.mouseY

    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    background: null

    contentItem: Item
    {
        implicitWidth: normalImage.implicitWidth
        implicitHeight: normalImage.implicitHeight

        Image
        {
            id: normalImage
            opacity: control.checked ? 0.0 : 1.0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
        Image
        {
            id: checkedImage
            opacity: 1.0 - normalImage.opacity
        }
    }

    MaterialEffect
    {
        anchors.fill: parent
        mouseArea: control
        rippleSize: 160
        clip: true
        radius: 2
    }

    MouseTracker
    {
        id: mouseTracker
        item: control
    }
}
