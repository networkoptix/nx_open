import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx.Items 1.0
import Nx 1.0

Popup
{
    id: calendarPanel

    property alias chunkProvider: calendar.chunkProvider
    property alias date: calendar.date

    signal datePicked(date date)

    signal opened()
    signal closed()

    readonly property int _animationDuration: 200

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside
    modal: true

    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight
    y: parent.height - implicitHeight
    padding: 0

    background: null

    Overlay.modal: Rectangle
    {
        color: ColorTheme.backgroundDimColor
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    contentItem: ArchiveCalendar
    {
        id: calendar
        layer.enabled: true

        onDatePicked: calendarPanel.datePicked(date)
        onCloseClicked: close()
    }

    enter: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                to: 1.0
            }

            NumberAnimation
            {
                target: contentItem
                property: "y"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                from: 56
                to: 0
            }
        }
    }

    exit: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                to: 0.0
            }

            NumberAnimation
            {
                target: contentItem
                property: "y"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                to: 56
            }
        }
    }

    onVisibleChanged:
    {
        if (visible)
            opened()
        else
            closed()
    }

    onOpened: contentItem.forceActiveFocus()
}
