import QtQuick 2.6
import Nx 1.0
import Nx.Animations 1.0

Item
{
    id: flickableView

    default property alias data: contentItem.data

    property real implicitContentWidth
    property real implicitContentHeight

    property alias contentWidth: contentItem.width
    property alias contentHeight: contentItem.height
    property alias contentX: contentItem.x
    property alias contentY: contentItem.y

    property real contentWidthPartToBeAlwaysVisible: 0
    property real contentHeightPartToBeAlwaysVisible: 0

    property bool alignToCenterWhenUnzoomed: false

    property Item zoomedItem: null

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

        property real minScale: 1
        readonly property real maxScale: 1000
        readonly property real scaleOvershoot: 1.2
        readonly property real scalePerDegree: 1.01
        readonly property real stickyScaleThreshold: 1.8
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

        function currentScale()
        {
            if (!haveImplicitSize)
                return 1

            return contentWidth / implicitContentWidth
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

        function defaultPanOffset()
        {
            if (!zoomedItem)
                return Qt.point(0, 0)

            var originX = (zoomedItem.x + zoomedItem.width / 2) / contentWidth
            var originY = (zoomedItem.y + zoomedItem.height / 2) / contentHeight

            return Qt.point(contentWidth * (0.5 - originX), contentHeight * (0.5 - originY))
        }

        function at_zoomedItemChangedGeometry()
        {
            if (mouseArea.buttons & Qt.RightButton
                || panOffsetXAnimation.running
                || panOffsetYAnimation.running
                || scaleAnimation.running)
            {
                return
            }

            fitInView(true)
        }
    }

    NumberAnimation
    {
        id: scaleAnimation
        easing.type: d.defaultAnimationEasingType
        target: scaleAnimation
        property: "scale"

        property real scale: 1

        onStarted: stickyScaleActivationTimer.stop()

        onStopped:
        {
            easing.type = d.defaultAnimationEasingType
            d.scalingAfterInteraction = false

            if (!mouseArea.pressed && !d.blockScaleBoundsAnimation)
            {
                if (scale < d.minScale)
                {
                    animateScaleTo(d.minScale, d.returnToBoundsDuration)
                }
                else if (scale > d.maxScale)
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

        onScaleChanged:
        {
            var newWidth = implicitContentWidth * scale
            var newHeight = implicitContentHeight * scale

            var ds = newWidth / contentWidth

            contentWidth = newWidth
            contentHeight = newHeight

            d.scaleOffsetX -= (d.scaleOriginX - 0.5) * newWidth * (ds - 1)
            d.scaleOffsetY -= (d.scaleOriginY - 0.5) * newHeight * (ds - 1)

            if (alignToCenterWhenUnzoomed && !panOffsetXAnimation.running && !panOffsetYAnimation.running
                && !(mouseArea.buttons & Qt.RightButton))
            {
                d.clearScaleOffset()

                var baseOffset = d.defaultPanOffset()

                var maxShiftX = Math.max(0, zoomedItem.width - width) / 2
                var maxShiftY = Math.max(0, zoomedItem.height - height) / 2

                d.panOffsetX = MathUtils.bound(
                    baseOffset.x - maxShiftX, d.panOffsetX, baseOffset.x + maxShiftX)
                d.panOffsetY = MathUtils.bound(
                    baseOffset.y - maxShiftY, d.panOffsetY, baseOffset.y + maxShiftY)
            }
        }
    }

    Binding
    {
        target: d
        property: "targetScale"
        value: d.currentScale()
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
            if (d.currentScale() < d.minScale * d.stickyScaleThreshold)
                animateScaleTo(d.minScale, d.stickyScaleAnimationDuration)
        }
    }

    Connections
    {
        target: zoomedItem

        onXChanged: d.at_zoomedItemChangedGeometry()
        onYChanged: d.at_zoomedItemChangedGeometry()
    }

    onWidthChanged: fitInView(true)

    onZoomedItemChanged: fitInView()

    function stopPanOffsetAnimations()
    {
        kineticAnimation.stop()
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

        var currentScale = d.currentScale()

        if (currentScale === scale)
            return

        if (duration === undefined)
            duration = d.defaultAnimationDuration

        scaleAnimation.from = currentScale
        scaleAnimation.to = scale
        scaleAnimation.duration = duration
        scaleAnimation.start()
    }

    function stopAnimations()
    {
        stopScaleAnimation()
        stopPanOffsetAnimations()
    }

    function returnToBounds(instant)
    {
        d.clearScaleOffset()

        var maxShiftX
        var maxShiftY
        var newX
        var newY

        if (zoomedItem && Utils.isChild(zoomedItem, contentItem))
        {
            var baseOffset = d.defaultPanOffset()

            maxShiftX = Math.max(0, zoomedItem.width - width) / 2
            maxShiftY = Math.max(0, zoomedItem.height - height) / 2

            newX = MathUtils.bound(
                baseOffset.x - maxShiftX, d.panOffsetX, baseOffset.x + maxShiftX)
            newY = MathUtils.bound(
                baseOffset.y - maxShiftY, d.panOffsetY, baseOffset.y + maxShiftY)
        }
        else
        {
            maxShiftX = (width + contentWidth) / 2
                - contentWidthPartToBeAlwaysVisible * width
            newX = MathUtils.bound(-maxShiftX, d.panOffsetX, maxShiftX)

            maxShiftY = (height + contentHeight) / 2
                - contentHeightPartToBeAlwaysVisible * height
            newY = MathUtils.bound(-maxShiftY, d.panOffsetY, maxShiftY)
        }

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

        d.blockScaleBoundsAnimation = true
        stopScaleAnimation()
        stopPanOffsetAnimations()
        d.blockScaleBoundsAnimation = false
        d.clearScaleOffset()

        var newScale = 1
        var newOffsetX = 0
        var newOffsetY = 0
        d.scaleOriginX = 0.5
        d.scaleOriginY = 0.5
        d.minScale = 1

        if (zoomedItem && Utils.isChild(zoomedItem, contentItem))
        {
            var originX = (zoomedItem.x + zoomedItem.width / 2) / contentWidth
            var originY = (zoomedItem.y + zoomedItem.height / 2) / contentHeight

            var newSize = MathUtils.scaledSize(
                Qt.size(zoomedItem.width, zoomedItem.height), Qt.size(width, height))
            newScale = d.currentScale() * newSize.width / zoomedItem.width
            d.minScale = newScale

            var newContentWidth = implicitContentWidth * newScale
            var newContentHeight = implicitContentHeight * newScale
            newOffsetX = newContentWidth * (0.5 - originX)
            newOffsetY = newContentHeight * (0.5 - originY)
        }

        if (instant)
        {
            scaleAnimation.scale = newScale
            d.panOffsetX = newOffsetX
            d.panOffsetY = newOffsetY
        }
        else
        {
            d.setAnimationEasingType(Easing.InOutCubic)
            animateScaleTo(newScale, d.fitInViewDuration)
            animatePanOffsetTo(newOffsetX, newOffsetY, d.fitInViewDuration)
        }
    }

    function setContentGeometry(x, y, width, height)
    {
        d.blockScaleBoundsAnimation = true
        stopScaleAnimation()
        stopPanOffsetAnimations()
        d.clearScaleOffset()
        d.blockScaleBoundsAnimation = false

        contentWidth = width
        contentHeight = height

        d.panOffsetX = x - contentX
        d.panOffsetY = y - contentY
    }
}
