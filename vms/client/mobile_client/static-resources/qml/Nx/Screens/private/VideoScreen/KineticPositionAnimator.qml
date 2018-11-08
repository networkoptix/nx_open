import QtQuick 2.6

QtObject
{
    id: animator

    readonly property point position: Qt.point(d.targetPosition.x, d.targetPosition.y)
    readonly property point initialPosition: Qt.point(d.initialPosition.x, d.initialPosition.y)

    readonly property alias inertia: speedVectorAnimation.running
    property alias inertiaDuration: speedVectorAnimation.duration
    property alias inertiaEasing: speedVectorAnimation.easing.type

    function interrupt()
    {
        if (d.lastPosition)
            finishMeasurement(d.lastPosition.x, d.lastPosition.y)
        d.animation.stop()
    }

    function startMeasurement(x, y)
    {
        speedVectorAnimation.stop()
        d.initialPosition = Qt.vector2d(x, y)
    }

    function updateMeasurement(x, y)
    {
        var newPosition = Qt.vector2d(x, y)
        var newPositionTimeMs = d.getCurrentTime()
        var hasDifferentLastPosition = d.currentPosition
            && d.currentPositionTimeMs != newPositionTimeMs

        if (hasDifferentLastPosition)
        {
            d.lastPosition = d.currentPosition
            d.lastPositionTimeMs = d.currentPositionTimeMs
        }

        d.currentPosition = newPosition
        d.currentPositionTimeMs = newPositionTimeMs

        d.targetPosition = newPosition
    }

    function finishMeasurement(x, y)
    {
        if (!d.lastPosition)
            return

        var timeAspect = 1 / (d.currentPositionTimeMs - d.lastPositionTimeMs)
        var newSpeedVector = d.currentPosition.minus(d.lastPosition).times(timeAspect)
        d.timestamp = d.getCurrentTime()
        speedVectorAnimation.from = newSpeedVector
        speedVectorAnimation.to = Qt.vector2d(0, 0)
        speedVectorAnimation.start()

        d.currentPosition = undefined
        d.lastPosition = undefined
    }

    readonly property QtObject d: QtObject
    {
        property vector2d initialPosition
        property vector2d targetPosition

        property var currentPosition
        property real currentPositionTimeMs

        property var lastPosition
        property real lastPositionTimeMs

        property vector2d speedVector

        property real timestamp

        onSpeedVectorChanged:
        {
            var currentTimestamp = getCurrentTime()
            targetPosition = targetPosition.plus(speedVector.times(currentTimestamp - timestamp))
            timestamp = currentTimestamp
        }

        function getCurrentTime()
        {
            return (new Date()).getTime()
        }

        readonly property PropertyAnimation animation: PropertyAnimation
        {
            id: speedVectorAnimation

            target: animator.d
            properties: "speedVector"
            duration: 2000
            easing.type: Easing.OutExpo
        }
    }
}
