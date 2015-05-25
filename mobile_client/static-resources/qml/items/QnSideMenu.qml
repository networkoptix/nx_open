import QtQuick 2.2
import Material 0.1

Item {
    id: rootItem

    property bool opened: !flickable.atXEnd
    property alias content: loader.sourceComponent
    property real menuWidth: width * 0.8

    property real __minVelocity: panel.width * 6

    Flickable {
        id: flickable

        width: opened ? rootItem.width : dragger.width
        height: parent.height

        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        contentWidth: width + panel.width
        contentX: panel.width

        clip: true

        onDragStarted: forceAnimation.stop()
        onDragEnded: finishTransition()
        onFlickEnded: finishTransition()
        onFlickStarted: finishTransition()

        Row {
            View {
                id: panel

                height: rootItem.height
                width: menuWidth

                elevation: 3

                Loader {
                    id: loader
                    anchors.fill: parent
                }
            }

            Rectangle {
                id: dragger
                height: rootItem.height
                width: rootItem.opened ? rootItem.width : units.dp(36)

                color: "black"
                opacity: rootItem.opened ? (panel.width - flickable.contentX) / panel.width * 0.4 : 0

                MouseArea {
                    anchors.fill: parent
                    enabled: rootItem.opened
                    onClicked: closeMenu()
                    propagateComposedEvents: true
                }
            }
        }
    }

    NumberAnimation {
        id: forceAnimation
        target: flickable
        property: "contentX"
        easing.type: Easing.Linear
    }

    function finishTransition() {
        forceAnimation.stop()

        if (flickable.atXBeginning || flickable.atXEnd)
            return

        var velocity = Math.max(Math.abs(flickable.horizontalVelocity), __minVelocity)
        var open = flickable.horizontalVelocity < 0

        flickable.cancelFlick()

        forceAnimation.to = open ? 0 : panel.width
        forceAnimation.duration = Math.abs(flickable.contentX - forceAnimation.to) / velocity * 1000
        forceAnimation.start()
    }

    function closeMenu() {
        forceAnimation.to = panel.width
        forceAnimation.duration = panel.width / rootItem.__minVelocity * 1000
        forceAnimation.start()
    }
}

