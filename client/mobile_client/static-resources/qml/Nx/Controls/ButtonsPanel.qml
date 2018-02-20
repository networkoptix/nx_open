import QtQuick 2.6

Row
{
    id: control

    property alias model: buttonRepeater.model

    signal buttonClicked(int index)

    layoutDirection: Qt.RightToLeft
    anchors.verticalCenter: parent.verticalCenter

    Repeater
    {
        id: buttonRepeater

        delegate:
            IconButton
            {
                anchors.verticalCenter: parent.verticalCenter
                icon: lp(model.iconPath)
                onClicked: control.buttonClicked(index)
            }
    }
}
