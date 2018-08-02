import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    id: calendar

    implicitWidth: mainWindow.width
    implicitHeight: column.implicitHeight

    property alias locale: dayOfWeekRow.locale
    property alias monthText: monthLabel.text
    property alias yearText: yearLabel.text
    readonly property alias monthsList: monthsList

    signal previousMonthClicked()
    signal nextMonthClicked()
    signal closeClicked()

    Column
    {
        id: column

        width: parent.width

        Item
        {
            height: 64
            width: parent.width

            IconButton
            {
                icon.source: lp("/images/arrow_left.png")
                onClicked: calendar.previousMonthClicked()
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
            }

            Row
            {
                spacing: 6
                anchors.centerIn: parent

                Text
                {
                    id: monthLabel

                    color: ColorTheme.windowText
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Text
                {
                    id: yearLabel

                    color: ColorTheme.windowText
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }

            IconButton
            {
                icon.source: lp("/images/arrow_right.png")
                onClicked: calendar.nextMonthClicked()
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
            }
        }

        DayOfWeekRow
        {
            id: dayOfWeekRow
            width: parent.width - 16
            x: 8
        }

        ListView
        {
            id: monthsList

            width: parent.width - 16
            x: 8
            height: 6 * 48
            clip: true
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            snapMode: ListView.SnapOneItem
        }

        Item
        {
            id: footer

            width: parent.width
            height: 64

            Button
            {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Close")
                icon.source: lp("/images/close.png")
                flat: true
                labelPadding: 8
                onClicked: calendar.closeClicked()
            }
        }
    }
}
