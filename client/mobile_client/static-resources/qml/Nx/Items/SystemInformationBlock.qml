import QtQuick 2.6
import Nx 1.0

import "private/SystemInformationBlock"

Item
{
    id: systemInformationBlock

    property string systemId
    property string localId
    property string systemName
    property string ownerDescription
    property bool cloud: false
    property bool online: true
    property bool reachable: true
    property string address
    property string user

    implicitHeight: column.height

    property alias topPadding: column.topPadding
    property alias bottomPadding: column.bottomPadding
    property alias leftPadding: column.leftPadding
    property alias rightPadding: column.rightPadding

    Column
    {
        id: column

        width: parent.width
        leftPadding: 16
        rightPadding: 16
        topPadding: 12
        bottomPadding: 12
        readonly property real availableWidth: width - leftPadding - rightPadding

        Text
        {
            text: systemName ? systemName : "<%1>".arg(qsTr("Unknown"))
            width: parent.width - 40
            height: 28
            font.pixelSize: 18
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: enabled ? ColorTheme.windowText : ColorTheme.base13
        }

        Loader
        {
            width: parent.availableWidth
            sourceComponent: cloud ? cloudSystemInformationComponent
                                   : localSystemInformationComponent
        }
    }

    Component
    {
        id: localSystemInformationComponent

        LocalSystemInformation
        {
            address: systemInformationBlock.address
            user: systemInformationBlock.user
        }
    }

    Component
    {
        id: cloudSystemInformationComponent

        CloudSystemInformation
        {
            description: systemInformationBlock.ownerDescription
        }
    }
}
