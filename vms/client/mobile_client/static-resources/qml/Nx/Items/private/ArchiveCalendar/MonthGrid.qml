import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

Item
{
    id: monthGrid

    property alias locale: calendarModel.locale
    property alias year: calendarModel.year
    property alias month: calendarModel.month
    property date currentDate
    property alias chunkProvider: calendarModel.chunkProvider

    property date _today: new Date()

    signal datePicked(date date)

    Grid
    {
        id: grid

        anchors.fill: parent

        columns: 7
        property real cellWidth: width / columns
        property real cellHeight: height / 6

        Repeater
        {
            id: repeater

            model: QnCalendarModel { id: calendarModel }

            Item
            {
                id: calendarDay

                property bool current: repeater.model.isCurrent(currentDate, model.display)

                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle
                {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    width: parent.width + 3
                    height: 3
                    radius: height / 2
                    color: ColorTheme.green_main
                    visible: model.hasArchive
                }

                Rectangle
                {
                    anchors.centerIn: parent
                    width: 40
                    height: 40
                    radius: width / 2
                    visible: calendarDay.current
                    color: ColorTheme.windowText
                }

                Text
                {
                    anchors.centerIn: parent
                    text: model.display
                    color: calendarDay.current ? ColorTheme.base3
                                               : model.date > _today ? ColorTheme.base15
                                                                     : ColorTheme.windowText
                    font.pixelSize: 16
                }

                MouseArea
                {
                    anchors.fill: parent
                    onClicked:
                    {
                        if (model.date.getMonth() + 1 !== calendarModel.month ||
                            model.date > _today)
                        {
                            return
                        }

                        currentDate = model.date
                        monthGrid.datePicked(model.date)
                    }
                }
            }
        }
    }
}

