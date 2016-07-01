import QtQuick 2.0

Item
{
    id: ripple

    property real centerX: 0
    property real centerY: 0
    property real size: 48
    property alias color: circle.color
    property bool deleteWhenFinished: true

    property int fadeInDuration: 200
    property int fadeOutDuration: 300
    property int scaleDuration: fadeInDuration + fadeOutDuration

    width: size
    height: size
    x: centerX - width / 2
    y: centerY - height / 2

    Rectangle
    {
        id: circle

        opacity: 0
        anchors.centerIn: parent
        height: width
        radius: width / 2

        NumberAnimation on width
        {
            id: sizeAnimation

            running: false
            from: 8
            to: size
            duration: scaleDuration
            easing.type: Easing.InOutQuad
        }

        NumberAnimation on opacity
        {
            id: fadeInAnimation

            running: false
            to: 1.0
            duration: fadeInDuration
            easing.type: Easing.InOutQuad
        }

        NumberAnimation on opacity
        {
            id: fadeOutAnimation

            running: false
            to: 0.0
            duration: fadeOutDuration
            easing.type: Easing.InOutQuad

            onStopped: ripple.destroy()
        }
    }

    function splash(x, y)
    {
        if (x != undefined && y != undefined)
        {
            centerX = x
            centerY = y
        }

        circle.opacity = 0
        circle.width = 8

        fadeInAnimation.stopped.connect(fadeOut)
        fadeInAnimation.start()
        sizeAnimation.start()
    }

    function fadeOut()
    {
        fadeInAnimation.stop()
        fadeOutAnimation.start()
    }

    function fadeIn(x, y)
    {
        if (x != undefined && y != undefined)
        {
            centerX = x
            centerY = y
        }

        circle.opacity = 0
        circle.width = 8

        scaleDuration = fadeInDuration

        fadeInAnimation.start()
        sizeAnimation.start()
    }
}
