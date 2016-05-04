import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Controls 1.0

Control
{
    id: control

    property string systemName
    property string host
    property int port
    property string version
    property bool compatible

    padding: 16

    implicitHeight: contentItem.implicitHeight + 2 * padding
    implicitWidth: 200

    background: Rectangle
    {
        color: compatible ? ColorTheme.base7 : ColorTheme.base6
        radius: 2

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            clip: true
            rippleSize: width * 0.8
        }
    }

    contentItem: Column
    {
        width: control.availableWidth

        Text
        {
            text: systemName ? systemName : "<%1>".arg(qsTr("Unknown"))
            width: parent.width
            height: 32
            font.pixelSize: 18
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: compatible ? ColorTheme.windowText : ColorTheme.base17
        }

        Text
        {
            text: host + ":" + port
            width: parent.width
            height: 24
            font.pixelSize: 15
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: compatible ? ColorTheme.contrast2 : ColorTheme.base17
        }

        Text
        {
            text: qsTr("incompatible server version") + (version ? ": " + version : "")
            width: parent.width
            height: 24
            font.pixelSize: 12
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: ColorTheme.orange_main
            visible: !compatible
        }
    }

    MouseArea
    {
        id: mouseArea

        enabled: compatible
        anchors.fill: parent
        onClicked: Workflow.openDiscoveredSession(systemName, host, port)
    }
}
