import QtQuick 2.6
import QtQuick.Controls 2.0
import Nx 1.0
import Nx.Controls 1.0

TextField
{
    id: control

    property alias delay: timer.interval
    readonly property string query: timer.query
    readonly property bool active: control.focus || control.text

    implicitWidth: 280
    implicitHeight: 28

    leftPadding: searchIcon.width + 1
    rightPadding: clearButton.width + 1

    background: Item
    {
        clip: true

        Rectangle
        {
            anchors.fill: parent
            color: control.active ? ColorTheme.colors.dark3 : ColorTheme.window
        }

        Rectangle
        {
            anchors
            {
                fill: parent
                topMargin: 1
                leftMargin: 0
                rightMargin: 0
                bottomMargin: -1
            }
            border.width: 2
            border.color: ColorTheme.colors.dark1
            radius: 1
            color: "transparent"
            visible: control.active
        }

        Rectangle
        {
            anchors.fill: parent
            color: "transparent"
            border.color: control.hovered ? ColorTheme.mid : ColorTheme.dark
        }

        Image
        {
            id: searchIcon
            source: "qrc:/skin/welcome_page/search.png"
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Button
    {
        id: clearButton

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 1

        height: parent.height - 2
        width: height

        flat: true
        backgroundColor: ColorTheme.colors.dark5
        iconUrl: "qrc:/skin/welcome_page/input_clear.png"

        visible: control.text

        onClicked:
        {
            control.forceActiveFocus()
            control.clear()
        }
    }

    Text
    {
        text: qsTr("Search")

        anchors.verticalCenter: parent.verticalCenter
        x: active ? control.leftPadding + 2 : (parent.width - width) / 2
        Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        visible: !control.text

        font.pixelSize: 13
        color: ColorTheme.windowText
    }

    // This MouseArea is a dirty ad-hoc solution for de-focusing the control by a click outside.
    // It will not work in many scenarios.
    // TODO: Replace with something more sane.
    MouseArea
    {
        anchors.fill: parent
        parent: control.parent
        visible: control.visible && control.active

        onPressed:
        {
            control.focus = false
            mouse.accepted = false
        }
    }

    onTextChanged: timer.restart()

    Timer
    {
        id: timer
        property string query
        onTriggered: query = control.text
    }
}
