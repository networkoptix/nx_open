import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    id: calendar

    property alias locale: dayOfWeekRow.locale
    property alias monthText: monthLabel.text
    property alias yearText: yearLabel.text
    readonly property alias monthsList: monthsList

    signal previousMonthClicked()
    signal nextMonthClicked()
    signal closeClicked()

    implicitWidth: header.width + column.width
    implicitHeight: column.implicitHeight

    Item
    {
        id: header

        height: parent.height
        width: 160

        Column
        {
            width: parent.width
            y: 16

            Text
            {
                id: yearLabel

                x: 16
                height: 24
                verticalAlignment: Text.AlignVCenter
                color: ColorTheme.windowText
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Text
            {
                id: monthLabel

                x: 16
                height: 32
                verticalAlignment: Text.AlignVCenter
                color: ColorTheme.windowText
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }

            Item
            {
                width: 1
                height: 16
            }

            Row
            {
                IconButton
                {
                    icon.source: lp("/images/arrow_left.png")
                    onClicked: calendar.previousMonthClicked()
                }

                IconButton
                {
                    icon.source: lp("/images/arrow_right.png")
                    onClicked: calendar.nextMonthClicked()
                }
            }
        }

        Button
        {
            anchors
            {
                bottom: parent.bottom
                bottomMargin: 6
            }

            labelPadding: 8
            text: qsTr("Close")
            icon.source: lp("/images/close.png")
            flat: true
            onClicked: calendar.closeClicked()
        }

        Rectangle
        {
            anchors.right: parent.right
            height: parent.height - 32
            width: 1
            y: 16
            color: ColorTheme.base9
        }
    }

    Column
    {
        id: column

        anchors.left: header.right
        width: 7 * 48 + 32
        spacing: 8
        padding: 16
        readonly property real availableWidth: width - leftPadding - rightPadding

        DayOfWeekRow
        {
            id: dayOfWeekRow
            width: parent.availableWidth
        }

        ListView
        {
            id: monthsList

            width: parent.availableWidth
            height: 6 * 40
            clip: true

            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            snapMode: ListView.SnapOneItem
        }
    }
}
