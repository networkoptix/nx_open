import QtQuick 2.6
import QtQuick.Layouts 1.1
import Nx 1.0

import "private/SystemInformationBlock"

Item
{
    id: systemInformationBlock

    property string systemId
    property string localId
    property string systemName
    property string ownerDescription
    property bool factorySystem: false
    property bool cloud: false
    property bool online: true
    property bool reachable: true
    property string address
    property string user
    property string factoryDetailsText

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

        Row
        {
            height: 28
            width: column.width

            Text
            {
                id: captionText

                text:
                {
                    if (factorySystem)
                        return qsTr("New Server")

                    return systemName ? systemName : "<%1>".arg(qsTr("Unknown"))
                }

                width: Math.min(column.availableWidth, implicitWidth)
                font.pixelSize: 18
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                color: enabled ? ColorTheme.windowText : ColorTheme.base13
                anchors.verticalCenter: parent.verticalCenter
            }

            Text
            {
                visible: factorySystem
                text: " – " + address
                width: column.availableWidth - captionText.width
                font.pixelSize: 16
                elide: Text.ElideRight
                color: ColorTheme.windowText
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 1
            }
        }

        Loader
        {
            width: column.availableWidth
            sourceComponent:
            {
                if (factorySystem)
                    return factorySystemInformationComponent
                else if (cloud)
                    return cloudSystemInformationComponent
                else
                    return localSystemInformationComponent
            }
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

    Component
    {
        id: factorySystemInformationComponent

        Text
        {
            topPadding: 8
            text: systemInformationBlock.factoryDetailsText
            wrapMode: Text.WordWrap
            width: parent.width
            font.pixelSize: 14
            color: ColorTheme.contrast10
        }
    }
}
