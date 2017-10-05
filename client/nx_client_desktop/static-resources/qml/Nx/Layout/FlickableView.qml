import QtQuick 2.6
import Nx 1.0
import Nx.Animations 1.0

Item
{
    id: flickableView

    default property alias data: contentItem.data

    readonly property alias contentWidth: contentItem.width
    readonly property alias contentHeight: contentItem.height
    readonly property alias contentX: contentItem.x
    readonly property alias contentY: contentItem.y

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
        property real targetScale: 0
        property bool blockScaleBoundsAnimation: false
        // Scale origin is in item coordinates scaled to 1x1.
        property real scaleOriginX: 0
        property real scaleOriginY: 0
        property real scaleOffsetX: 0
        property real scaleOffsetY: 0

        property real contentAspectRatio: 0
        readonly property bool haveContentSize: contentAspectRatio > 0

        property real panOffsetX: 0
        property real panOffsetY: 0

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
            if (!haveContentSize)
                return 1

            return contentWidth / d.implicitContentSize().width
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
                return Qt.point((width - contentWidth) / 2, (height - contentHeight) / 2)

            return Qt.point(
                -zoomedItem.x + (width - zoomedItem.width) / 2,
                -zoomedItem.y + (height - zoomedItem.height) / 2)
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

        function implicitContentSize()
        {
            return haveContentSize
                ? MathUtils.scaledSize(Qt.size(contentAspectRatio, 1), Qt.size(width, height))
                : Qt.size(0, 0)
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
            d.targetScale = 0

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
            var x = contentX
            var y = contentY
            var w = contentItem.width
            var h = contentItem.height

            var implicitContentSize = d.implicitContentSize()
            contentItem.width = implicitContentSize.width * scaleAnimation.scale
            contentItem.height = implicitContentSize.height * scaleAnimation.scale

            d.scaleOffsetX += d.scaleOriginX * (w - contentItem.width)
            d.scaleOffsetY += d.scaleOriginY * (h - contentItem.height)

            if (alignToCenterWhenUnzoomed
                && !panOffsetXAnimation.running && !panOffsetYAnimation.running
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

            d.panOffsetX = MathUtils.bound(
                -contentWidth,
                pressPanOffsetX + mouse.x - pressX,
                width)

            d.panOffsetY = MathUtils.bound(
                -contentHeight,
                pressPanOffsetY + mouse.y - pressY,
                height)

            kineticAnimation.continueMeasurement(Qt.point(d.panOffsetX, d.panOffsetY))
        }

        onClicked: flickableView.clicked(mouse.x, mouse.y, mouse.button)
        onDoubleClicked: flickableView.doubleClicked(mouse.x, mouse.y, mouse.button)

        onWheel:
        {
            if (!d.haveContentSize)
                return

            var angle = wheel.angleDelta.y / 8

            var scale = d.targetScale !== 0 ? d.targetScale : d.currentScale()

            var newScale = MathUtils.bound(
                d.minScale / d.scaleOvershoot,
                scale * Math.pow(d.scalePerDegree, angle),
                d.maxScale * d.scaleOvershoot)

            d.blockScaleBoundsAnimation = true

            stopScaleAnimation()
            stopPanOffsetAnimations()

            d.blockScaleBoundsAnimation = false

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

            if (d.panOffsetX < -contentWidth || d.panOffsetX > width
                || d.panOffsetY < -contentHeight || d.panOffsetY > height)
            {
                stop()
            }
        }

        onStopped: returnToBounds()
    }

    Item
    {
        id: contentItem
        x: d.panOffsetX + d.scaleOffsetX
        y: d.panOffsetY + d.scaleOffsetY
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
    onHeightChanged: fitInView(true)

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
            var minVisibleWidth = contentWidthPartToBeAlwaysVisible * width
            newX = MathUtils.bound(
                -contentWidth + minVisibleWidth, d.panOffsetX, width - minVisibleWidth)

            var minVisibleHeight = contentHeightPartToBeAlwaysVisible * height
            newY = MathUtils.bound(
                -contentHeight + minVisibleHeight, d.panOffsetY, height - minVisibleHeight)
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
        if (!d.haveContentSize)
            return

        stickyScaleActivationTimer.stop()

        d.blockScaleBoundsAnimation = true
        stopScaleAnimation()
        stopPanOffsetAnimations()
        d.blockScaleBoundsAnimation = false
        d.clearScaleOffset()

        var implicitContentSize = d.implicitContentSize()

        var newScale = 1
        var newOffsetX = (width - implicitContentSize.width) / 2
        var newOffsetY = (height - implicitContentSize.height) / 2
        d.scaleOriginX = 0
        d.scaleOriginY = 0
        d.minScale = 1

        if (zoomedItem && Utils.isChild(zoomedItem, contentItem))
        {
            var originX = (zoomedItem.x + zoomedItem.width / 2) / contentWidth
            var originY = (zoomedItem.y + zoomedItem.height / 2) / contentHeight

            var newSize = MathUtils.scaledSize(
                Qt.size(zoomedItem.width, zoomedItem.height), Qt.size(width, height))
            newScale = d.currentScale() * newSize.width / zoomedItem.width
            d.minScale = newScale

            var newContentWidth = implicitContentSize.width * newScale
            var newContentHeight = implicitContentSize.height * newScale
            newOffsetX = -zoomedItem.x / contentWidth * newContentWidth
                + (width - newSize.width) / 2
            newOffsetY = -zoomedItem.y / contentHeight * newContentHeight
                + (height - newSize.height) / 2
        }

        if (instant)
        {
            scaleAnimation.scale = newScale
            d.panOffsetX = newOffsetX
            d.panOffsetY = newOffsetY
            contentItem.width = implicitContentSize.width
            contentItem.height = implicitContentSize.height
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

        contentItem.width = width
        contentItem.height = height

        d.contentAspectRatio = (width === 0 || height === 0) ? 0 : width / height

        d.panOffsetX = x
        d.panOffsetY = y
    }

    function implicitContentGeometry(aspectRatio)
    {
        if (aspectRatio === 0)
            return Qt.rect(0, 0, 0, 0)

        if (width === 0 || height === 0)
            return Qt.rect(0, 0, aspectRatio, 1)

        var size = MathUtils.scaledSize(Qt.size(aspectRatio, 1), Qt.size(width, height))
        return Qt.rect(
            (width - size.width) / 2,
            (height - size.height) / 2,
            size.width,
            size.height)
    }
}
