import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Button
{
    id: control

    property bool selected: false

    property alias image: normalImage.source
    property alias selectedImage: selectedImage.source

    property real mouseX: mouseTracker.mouseX
    property real mouseY: mouseTracker.mouseY

    implicitWidth: label.implicitWidth + leftPadding + rightPadding
    implicitHeight: label.implicitHeight + topPadding + bottomPadding

    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    background: null

    label: Item
    {
        implicitWidth: normalImage.implicitWidth
        implicitHeight: normalImage.implicitHeight

        Image
        {
            id: normalImage
            opacity: control.selected ? 0.0 : 1.0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
        Image
        {
            id: selectedImage
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

    ItemMouseTracker
    {
        id: mouseTracker
        item: control
    }
}
