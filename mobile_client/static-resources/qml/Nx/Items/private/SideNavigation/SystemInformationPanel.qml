import QtQuick 2.6
import QtQuick.Layouts 1.1
import Qt.labs.controls 1.0
import Nx 1.0

Pane
{
    id: systemInformationPanel

    implicitWidth: parent ? parent.width : contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    padding: 16

    background: Item
    {
        Rectangle
        {
            width: parent.width - 32
            height: 1
            x: 16
            anchors.bottom: parent.bottom
            color: ColorTheme.base10
        }
    }

    contentItem: Column
    {
        width: systemInformationPanel.availableWidth

        Text
        {
            width: parent.width
            height: 28
            text: connectionManager.systemName
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 18
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            color: ColorTheme.windowText
        }

        Loader
        {
            sourceComponent: localSystemInformationComponent
        }
    }

    Component
    {
        id: localSystemInformationComponent

        Column
        {
            width: systemInformationPanel.availableWidth
            topPadding: 6
            spacing: 2

            RowLayout
            {
                width: parent.width
                spacing: 2

                Rectangle
                {
                    width: 20
                    height: 20
                    color: "yellow"
                }

                Text
                {
                    Layout.fillWidth: true
                    text: connectionManager.currentHost + ":" + connectionManager.currentPort
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    color: ColorTheme.contrast4
                }
            }

            RowLayout
            {
                width: parent.width
                spacing: 2

                Rectangle
                {
                    width: 20
                    height: 20
                    color: "yellow"
                }

                Text
                {
                    Layout.fillWidth: true
                    text: connectionManager.currentLogin
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    color: ColorTheme.contrast4
                }
            }
        }
    }

    Component
    {
        id: cloudSystemInformationComponent

        Column
        {
            width: systemInformationPanel.availableWidth
            spacing: 4

            Text
            {
                text: "Demo Demov's system" // TODO: #dklychkov Implement
                width: parent.width
                height: 20
                font.pixelSize: 14
                elide: Text.ElideRight
                color: ColorTheme.windowText
            }

            Rectangle
            {
                width: 24
                height: 24
                color: "yellow"
            }
        }
    }
}
