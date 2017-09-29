import QtQuick 2.6
import Nx 1.0
import Nx.Animations 1.0

Item
{
    id: flickableView

    default property alias data: contentItem.data
    readonly property alias contentItem: contentItem

    property real implicitContentWidth
    property real implicitContentHeight

    property alias contentWidth: contentItem.width
    property alias contentHeight: contentItem.height
    property alias contentX: contentItem.x
    property alias contentY: contentItem.y

    property real contentWidthPartToBeAlwaysVisible: 0
    property real contentHeightPartToBeAlwaysVisible: 0

    signal clicked(int x, int y, int button)
    signal doubleClicked(int x, int y, int button)

    QtObject
    {
        id: d

        readonly property int defaultAnimationEasingType: Easing.OutCubic
        readonly property int defaultAnimationDuration: 200
        readonly property int returnToBoundsDuration: 700
        readonly property int fitInViewDuration: 500
        readonly property int zoomAnimationDuration: 1000
        readonly property int zoomOvershootAnimationDuration: 200
        readonly property int stickyScaleAnimationDuration: 3000

        readonly property real minScale: 1
        readonly property real maxScale: 1000
        readonly property real scaleOvershoot: 1.2
        readonly property real scalePerDegree: 1.01
        readonly property real stickyScaleThreshold: 1.8
        property real scale: 1
        property real targetScale: 1
        property bool blockScaleBoundsAnimation: false
        property bool scalingAfterInteraction: false
        // Scale origin is in item coordinates scaled to 1x1.
        property real scaleOriginX: 0.5
        property real scaleOriginY: 0.5
        property real scaleOffsetX: 0
        property real scaleOffsetY: 0

        property real panOffsetX: 0
        property real panOffsetY: 0

        readonly property bool haveImplicitSize:
            implicitContentWidth > 0 && implicitContentHeight > 0

        NumberAnimation on panOffsetX
        {
            id: panOffsetXAnimation
            easing.type: d.defaultAnimationEasingType

            onStopped: easing.type = d.defaultAnimationEasingType
        }

        NumberAnimation on panOffsetY
        {
            id: panOffsetYAnimation
            easing.type: d.defaultAnimationEasingType

            onStopped: easing.type = d.defaultAnimationEasingType
        }

        NumberAnimation on scale
        {
            id: scaleAnimation
            easing.type: d.defaultAnimationEasingType

            onStarted: stickyScaleActivationTimer.stop()

            onStopped:
            {
                easing.type = d.defaultAnimationEasingType
                d.scalingAfterInteraction = false

                if (!mouseArea.pressed && !d.blockScaleBoundsAnimation)
                {
                    if (d.scale < d.minScale)
                    {
                        animateScaleTo(d.minScale, d.returnToBoundsDuration)
                    }
                    else if (d.scale > d.maxScale)
                    {
                        animateScaleTo(d.maxScale, d.returnToBoundsDuration)
                    }
                    else
                    {
                        returnToBounds()
                        stickyScaleActivationTimer.restart()
                    }
                }
            }
        }

        onScaleChanged:
        {
            var newWidth = implicitContentWidth * scale
            var newHeight = implicitContentHeight * scale

            var ds = newWidth / contentWidth

            contentWidth = newWidth
            contentHeight = newHeight

            scaleOffsetX -= (scaleOriginX - 0.5) * newWidth * (ds - 1)
            scaleOffsetY -= (scaleOriginY - 0.5) * newHeight * (ds - 1)
        }

        function setAnimationEasingType(easingType)
        {
            panOffsetXAnimation.easing.type = easingType
            panOffsetYAnimation.easing.type = easingType
            scaleAnimation.easing.type = easingType
        }

        function clearScaleOffset()
        {
            panOffsetX += scaleOffsetX
            mouseArea.pressPanOffsetX += scaleOffsetX
            panOffsetY += scaleOffsetY
            mouseArea.pressPanOffsetY += scaleOffsetY
            scaleOffsetX = 0
            scaleOffsetY = 0
        }
    }

    Binding
    {
        target: d
        property: "targetScale"
        value: d.scale
        when: !d.scalingAfterInteraction
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property real pressX
        property real pressY
        property real pressPanOffsetX
        property real pressPanOffsetY

        onPressed:
        {
            if (mouse.button !== Qt.RightButton)
                return

            stopPanOffsetAnimations()

            pressX = mouse.x
            pressY = mouse.y
            pressPanOffsetX = d.panOffsetX
            pressPanOffsetY = d.panOffsetY

            kineticAnimation.startMeasurement(Qt.point(d.panOffsetX, d.panOffsetY))
        }

        onReleased:
        {
            if (mouse.button !== Qt.RightButton)
                return

            d.clearScaleOffset()

            d.panOffsetX = pressPanOffsetX + mouse.x - pressX
            d.panOffsetY = pressPanOffsetY + mouse.y - pressY

            kineticAnimation.finishMeasurement(Qt.point(d.panOffsetX, d.panOffsetY))
        }

        onPositionChanged:
        {
            if (mouse.buttons !== Qt.RightButton)
                return

            d.clearScaleOffset()

            var maxShiftX = (width + contentWidth) / 2
            d.panOffsetX = MathUtils.bound(
                -maxShiftX,
                pressPanOffsetX + mouse.x - pressX,
                maxShiftX)

            var maxShiftY = (height + contentHeight) / 2
            d.panOffsetY = MathUtils.bound(
                -maxShiftY,
                pressPanOffsetY + mouse.y - pressY,
                maxShiftY)

            kineticAnimation.continueMeasurement(Qt.point(d.panOffsetX, d.panOffsetY))
        }

        onClicked: flickableView.clicked(mouse.x, mouse.y, mouse.button)
        onDoubleClicked: flickableView.doubleClicked(mouse.x, mouse.y, mouse.button)

        onWheel:
        {
            if (!d.haveImplicitSize)
                return

            var angle = wheel.angleDelta.y / 8

            var newScale = MathUtils.bound(
                d.minScale / d.scaleOvershoot,
                d.targetScale * Math.pow(d.scalePerDegree, angle),
                d.maxScale * d.scaleOvershoot)

            d.blockScaleBoundsAnimation = true

            stopScaleAnimation()
            stopPanOffsetAnimations()

            d.blockScaleBoundsAnimation = false

            d.scalingAfterInteraction = true
            d.scaleOriginX = (-contentX + wheel.x) / contentWidth
            d.scaleOriginY = (-contentY + wheel.y) / contentHeight
            d.targetScale = newScale
            animateScaleTo(
                newScale,
                newScale >= d.minScale && newScale <= d.maxScale
                    ? d.zoomAnimationDuration
                    : d.zoomOvershootAnimationDuration)
        }
    }

    KineticAnimation
    {
        id: kineticAnimation

        onPositionChanged:
        {
            d.panOffsetX = position.x
            d.panOffsetY = position.y

            if (mouseArea.pressedButtons & Qt.RightButton || d.blockScaleBoundsAnimation)
                return

            var maxShiftX = (width + contentWidth) / 2
            var maxShiftY = (height + contentHeight) / 2
            if (Math.abs(d.panOffsetX) >= maxShiftX || Math.abs(d.panOffsetY) >= maxShiftY)
                stop()
        }

        onStopped: returnToBounds()
    }

    Item
    {
        id: contentItem

        Binding
        {
            target: contentItem
            property: "x"
            value: width / 2 - contentWidth / 2 + d.panOffsetX + d.scaleOffsetX
        }
        Binding
        {
            target: contentItem
            property: "y"
            value: height / 2 - contentHeight / 2 + d.panOffsetY + d.scaleOffsetY
        }
    }

    Timer
    {
        id: stickyScaleActivationTimer

        interval: 200
        onTriggered:
        {
            if (d.scale < d.stickyScaleThreshold)
                animateScaleTo(1, d.stickyScaleAnimationDuration)
        }
    }

    onWidthChanged: fitInView(true)

    function stopPanOffsetAnimations()
    {
        panOffsetXAnimation.stop()
        panOffsetYAnimation.stop()
    }

    function animatePanOffsetTo(x, y, duration)
    {
        stopPanOffsetAnimations()

        if (d.panOffsetX === x && d.panOffsetY ===y)
            return

        if (duration === undefined)
            duration = d.defaultAnimationDuration

        panOffsetXAnimation.to = x
        panOffsetYAnimation.to = y
        panOffsetXAnimation.duration = duration
        panOffsetYAnimation.duration = duration
        panOffsetXAnimation.start()
        panOffsetYAnimation.start()
    }

    function stopScaleAnimation()
    {
        scaleAnimation.stop()
    }

    function animateScaleTo(scale, duration)
    {
        stopScaleAnimation()

        if (d.scale === scale)
            return

        if (duration === undefined)
            duration = d.defaultAnimationDuration

        scaleAnimation.to = scale
        scaleAnimation.duration = duration
        scaleAnimation.start()
    }

    function returnToBounds(instant)
    {
        d.clearScaleOffset()

        var maxShiftX = (width + contentWidth) / 2
            - contentWidthPartToBeAlwaysVisible * width
        var newX = MathUtils.bound(-maxShiftX, d.panOffsetX, maxShiftX)

        var maxShiftY = (height + contentHeight) / 2
            - contentHeightPartToBeAlwaysVisible * height
        var newY = MathUtils.bound(-maxShiftY, d.panOffsetY, maxShiftY)

        if (instant)
        {
            d.panOffsetX = newX
            d.panOffsetY = newY
        }
        else
        {
            animatePanOffsetTo(newX, newY, d.returnToBoundsDuration)
        }
    }

    function fitInView(instant)
    {
        if (!d.haveImplicitSize)
            return

        stickyScaleActivationTimer.stop()

        if (instant)
        {
            d.scale = 0
            d.panOffsetX = 0
            d.panOffsetY = 0
            d.scaleOffsetX = 0
            d.scaleOffsetY = 0
        }
        else
        {
            d.blockScaleBoundsAnimation = true
            kineticAnimation.stop()
            stopScaleAnimation()
            stopPanOffsetAnimations()
            d.blockScaleBoundsAnimation = false
            d.clearScaleOffset()
            d.scaleOriginX = 0.5
            d.scaleOriginY = 0.5
            d.setAnimationEasingType(Easing.InOutCubic)
            animateScaleTo(1, d.fitInViewDuration)
            animatePanOffsetTo(0, 0, d.fitInViewDuration)
        }
    }
}
