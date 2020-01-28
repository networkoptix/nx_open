import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Controls 1.0

Control
{
    id: informationPanel

    property var engineInfo

    property bool checkable: true
    property bool checked: false

    signal clicked()

    padding: 12
    Layout.fillWidth: true

    background: Rectangle { color: ColorTheme.colors.dark8 }

    contentItem: ColumnLayout
    {
        id: column
        spacing: 0

        RowLayout
        {
            id: caption
            spacing: 0

            Text
            {
                id: name

                text: engineInfo ? engineInfo.name : ""
                color: ColorTheme.text
                font.pixelSize: 15
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            SwitchIcon
            {
                id: enableSwitch

                visible: informationPanel.checkable
                hovered: mouseArea.containsMouse && !mouseArea.containsPress
                checkState: informationPanel.checked ? Qt.Checked : Qt.Unchecked

                MouseArea
                {
                    id: mouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: true
                    onClicked: informationPanel.clicked()
                }
            }
        }

        Text
        {
            id: description
            text: engineInfo ? engineInfo.description : ""
            color: ColorTheme.windowText
            wrapMode: Text.Wrap
            topPadding: 8
        }

        Item
        {
            id: spacing
            implicitHeight: 16
            visible: informationTable.height > 0
        }

        GridLayout
        {
            id: informationTable

            columns: 2
            columnSpacing: 8
            rowSpacing: 8
            flow: GridLayout.LeftToRight

            Text
            {
                text: qsTr("Version")
                color: ColorTheme.windowText
                visible: version.visible
            }

            Text
            {
                id: version
                text: engineInfo ? engineInfo.version : ""
                color: ColorTheme.light
                visible: !!text
            }

            Text
            {
                text: qsTr("Vendor")
                color: ColorTheme.windowText
                visible: vendor.visible
            }

            Text
            {
                id: vendor
                text: engineInfo ? engineInfo.vendor : ""
                color: ColorTheme.light
                visible: !!text
            }
        }
    }
}
