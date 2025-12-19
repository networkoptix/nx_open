// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Item
{
    id: transitionOverlay

    enum ZoomDirection { LessColumns, Invalid, MoreColumns }

    signal userDefinedColumnsCountChanged()

    readonly property int kInvalidCellIndex: -1
    readonly property int kInvalidColumnsCount: -1

    property bool layoutTransitionRunning: false
    property int centerpieceCellIndexForPinch: kInvalidCellIndex

    property bool grabbingInProgress: false

    property int startColumnsCount: kInvalidColumnsCount
    property int previousColumnsCount: kInvalidColumnsCount
    property int currentColumnsCount: kInvalidColumnsCount

    property int beforeColumnsCount: kInvalidColumnsCount
    property int afterColumnsCount: kInvalidColumnsCount

    visible: false

    function animateWheelLayoutChange(newColumnsCountUserModifier)
    {
        if (layoutTransitionRunning)
            return

        const newUserDefinedColumnsCount =
            sizesCalculator.defaultColumnsCount + newColumnsCountUserModifier

        if (newUserDefinedColumnsCount < 1
            || newUserDefinedColumnsCount > sizesCalculator.defaultColumnsCount
            || newUserDefinedColumnsCount === sizesCalculator.getUserDefinedColumnsCount())
        {
            return
        }

        layoutTransitionRunning = true

        flickable.grabToImage(function(resultBefore)
        {
            if (!resultBefore || !resultBefore.url)
            {
                layoutTransitionRunning = false
                return
            }

            beforeImage.source = resultBefore.url
            beforeImage.opacity = 1.0
            afterImage.opacity = 0.0

            transitionOverlay.visible = true

            let centerpieceCellIndexForCurrentWheelStep = d.getCenterpieceCellIndex()
            sizesCalculator.setUserDefinedColumnsCount(newUserDefinedColumnsCount)
            transitionOverlay.userDefinedColumnsCountChanged()

            Qt.callLater(
                function()
                {
                    flickable.contentY =
                        d.calculateFlickableContentY(centerpieceCellIndexForCurrentWheelStep)
                    flickable.grabToImage(function(resultAfter)
                    {
                        if (!resultAfter || !resultAfter.url)
                        {
                            layoutTransitionRunning = false
                            return
                        }

                        afterImage.source = resultAfter.url

                        wheelCrossFadeAnimation.start()
                    })
                })
        })
    }

    function finalizePinchWithAnimation()
    {
        startColumnsCount = kInvalidColumnsCount
        sizesCalculator.setUserDefinedColumnsCount(beforeImage.opacity < 0.5
            ? afterColumnsCount
            : beforeColumnsCount)

        transitionOverlay.userDefinedColumnsCountChanged()

        currentColumnsCount = kInvalidColumnsCount
        beforeColumnsCount = kInvalidColumnsCount
        afterColumnsCount = kInvalidColumnsCount
        layoutTransitionRunning = true
        pinchCrossFadeAnimation.start()
    }

    function animatePinchLayoutChange(scale)
    {
        if (grabbingInProgress || layoutTransitionRunning)
            return

        if (startColumnsCount === kInvalidColumnsCount)
        {
            CoreUtils.assert(previousColumnsCount === kInvalidColumnsCount
                    && currentColumnsCount === kInvalidColumnsCount
                    && beforeColumnsCount === kInvalidColumnsCount
                    && afterColumnsCount === kInvalidColumnsCount,
                "All the modifiers stored values must be invalid at the same time")

            startColumnsCount = sizesCalculator.getUserDefinedColumnsCount()
            previousColumnsCount = startColumnsCount
            currentColumnsCount = startColumnsCount
            beforeColumnsCount = startColumnsCount
            afterColumnsCount = startColumnsCount
        }

        let [relativeModifier, transitionProgress] =
            d.calculateRelativeModifierAndTransitionProgress(scale)

        let newColumnsCount = startColumnsCount + relativeModifier
        if (newColumnsCount !== currentColumnsCount)
        {
            previousColumnsCount = currentColumnsCount
            currentColumnsCount = newColumnsCount
        }

        CoreUtils.assert(previousColumnsCount !== kInvalidColumnsCount
                && currentColumnsCount !== kInvalidColumnsCount
                && beforeColumnsCount !== kInvalidColumnsCount
                && afterColumnsCount !== kInvalidColumnsCount,
            "All the modifiers stored values must be valid")

        let currentDirection = TransitionOverlay.ZoomDirection.Invalid
        if (newColumnsCount < previousColumnsCount)
            currentDirection = TransitionOverlay.ZoomDirection.LessColumns
        else if (newColumnsCount > previousColumnsCount)
            currentDirection = TransitionOverlay.ZoomDirection.MoreColumns
        else
            CoreUtils.assert(false, "Invalid state, currentColumnsCount === previousColumnsCount")

        const initialDirection = scale < 1
             ? TransitionOverlay.ZoomDirection.MoreColumns
             : TransitionOverlay.ZoomDirection.LessColumns

        if (currentDirection !== initialDirection)
        {
            transitionProgress = 1 - transitionProgress
            if (initialDirection === TransitionOverlay.ZoomDirection.LessColumns)
                newColumnsCount += 1
            else if (initialDirection === TransitionOverlay.ZoomDirection.MoreColumns)
                newColumnsCount -= 1
            else
                CoreUtils.assert(false, "Invalid initial or current direction")
        }

        if (newColumnsCount < 1 || newColumnsCount > sizesCalculator.defaultColumnsCount)
            return

        d.animatePinch(currentDirection, newColumnsCount, transitionProgress)
    }

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.colors.dark4
    }

    Image
    {
        id: beforeImage

        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        opacity: 1.0
    }

    Image
    {
        id: afterImage

        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        opacity: 0.0
    }

    SequentialAnimation
    {
        id: pinchCrossFadeAnimation

        readonly property int kAnimationDuration: 500

        running: false

        ParallelAnimation
        {
            NumberAnimation
            {
                target: beforeImage
                property: "opacity"
                from: beforeImage.opacity
                to: beforeImage.opacity < 0.5 ? 0 : 1
                duration: pinchCrossFadeAnimation.kAnimationDuration
                easing.type: Easing.InOutQuad
            }

            NumberAnimation
            {
                target: afterImage
                property: "opacity"
                from: afterImage.opacity
                to: beforeImage.opacity < 0.5 ? 1 : 0
                duration: pinchCrossFadeAnimation.kAnimationDuration
                easing.type: Easing.InOutQuad
            }
        }

        ScriptAction
        {
            script:
            {
                transitionOverlay.visible = false
                transitionOverlay.layoutTransitionRunning = false
                centerpieceCellIndexForPinch = kInvalidCellIndex
            }
        }
    }

    SequentialAnimation
    {
        id: wheelCrossFadeAnimation

        readonly property int kAnimationDuration: 500

        running: false

        ParallelAnimation
        {
            NumberAnimation
            {
                target: beforeImage
                property: "opacity"
                from: 1
                to: 0
                duration: wheelCrossFadeAnimation.kAnimationDuration
                easing.type: Easing.InOutQuad
            }

            NumberAnimation
            {
                target: afterImage
                property: "opacity"
                from: 0
                to: 1
                duration: wheelCrossFadeAnimation.kAnimationDuration
                easing.type: Easing.InOutQuad
            }
        }

        ScriptAction
        {
            script:
            {
                transitionOverlay.visible = false
                transitionOverlay.layoutTransitionRunning = false
            }
        }
    }

    QtObject
    {
        id: d

        function calculateTransitionProgress(scale, leftBorderValue, rightBorderValue)
        {
            return (scale - leftBorderValue) / (rightBorderValue - leftBorderValue)
        }

        function calculateRelativeModifierAndTransitionProgress(scale)
        {
            let relativeModifier = 0
            let transitionProgress = 0

            const kBorderValue_1 = 0.67
            const kBorderValue_2 = 0.36
            const kBorderValue_3 = 0.15

            if (scale < 1)
            {
                if (scale > kBorderValue_1)
                {
                    relativeModifier = 1
                    transitionProgress =
                        calculateTransitionProgress(scale, kBorderValue_1, 1)
                }
                else if (scale > kBorderValue_2)
                {
                    relativeModifier = 2
                    transitionProgress =
                        calculateTransitionProgress(scale, kBorderValue_2, kBorderValue_1)
                }
                else if (scale > kBorderValue_3)
                {
                    relativeModifier = 3
                    transitionProgress =
                        calculateTransitionProgress(scale, kBorderValue_3, kBorderValue_2)
                }
                else
                {
                    relativeModifier = 3
                    transitionProgress = 0
                }
            }
            else
            {
                relativeModifier = -1 * Math.floor(scale / 1)
                transitionProgress = 1 - scale % 1
            }

            return [relativeModifier, transitionProgress]
        }

        function animatePinch(currentDirection, currentColumnsCount, transitionProgress)
        {
            if (currentDirection === TransitionOverlay.ZoomDirection.LessColumns)
            {
                if (currentColumnsCount === afterColumnsCount
                    && currentColumnsCount + 1 === beforeColumnsCount)
                {
                    beforeImage.opacity = transitionProgress
                    afterImage.opacity = 1.0 - transitionProgress
                }
                else
                {
                    grabPinchImages(currentColumnsCount + 1, currentColumnsCount)
                }
            }
            else if (currentDirection === TransitionOverlay.ZoomDirection.MoreColumns)
            {
                if (currentColumnsCount === afterColumnsCount
                    && currentColumnsCount - 1 === beforeColumnsCount)
                {
                    beforeImage.opacity = transitionProgress
                    afterImage.opacity = 1.0 - transitionProgress
                }
                else
                {
                    grabPinchImages(currentColumnsCount - 1, currentColumnsCount)
                }
            }
            else
            {
                CoreUtils.assert(false, "Invalid direction in animation")
            }
        }

        function grabPinchImages(newBeforeColumnsCount, newAfterColumnsCount)
        {
            grabbingInProgress = true

            CoreUtils.assert(newBeforeColumnsCount !== newAfterColumnsCount,
                "Equal newBefore and newAfter modifiers")

            let grabImages =
                function()
                {
                    flickable.grabToImage(function(resultBefore)
                    {
                        if (!resultBefore || !resultBefore.url)
                        {
                            grabbingInProgress = false
                            return
                        }

                        beforeImage.source = resultBefore.url
                        beforeImage.opacity = 1.0
                        afterImage.opacity = 0.0
                        transitionOverlay.visible = true

                        beforeColumnsCount = newBeforeColumnsCount

                        if (centerpieceCellIndexForPinch === kInvalidCellIndex)
                            centerpieceCellIndexForPinch = getCenterpieceCellIndex()

                        sizesCalculator.setUserDefinedColumnsCount(newAfterColumnsCount)

                        afterColumnsCount = kInvalidColumnsCount
                        Qt.callLater(
                            function()
                            {
                                flickable.contentY =
                                    calculateFlickableContentY(centerpieceCellIndexForPinch)
                                flickable.grabToImage(function(resultAfter)
                                {
                                    if (!resultAfter || !resultAfter.url)
                                    {
                                        grabbingInProgress = false
                                        return
                                    }

                                    afterImage.source = resultAfter.url
                                    afterColumnsCount = newAfterColumnsCount

                                    grabbingInProgress = false
                                })
                            })
                    })
                }

            if (newBeforeColumnsCount === sizesCalculator.getUserDefinedColumnsCount())
            {
                grabImages()
            }
            else
            {
                sizesCalculator.setUserDefinedColumnsCount(newBeforeColumnsCount)
                Qt.callLater(grabImages)
            }
        }

        function getCenterpieceCellIndex()
        {
            const flickableCenterY = flickable.contentY + Math.min(flickable.height, flickable.contentHeight) / 2
            var itemsAtVerticalCenter = []
            for (let i = 0; i < repeater.count; ++i)
            {
                const item = repeater.itemAt(i)
                if (item)
                {
                    const itemTop = item.y
                    const itemBottom = item.y + item.height + sizesCalculator.spacing
                    if (flickableCenterY >= itemTop && flickableCenterY <= itemBottom)
                        itemsAtVerticalCenter.push(i)
                    if (itemTop > flickableCenterY)
                        break
                }
            }

            if (itemsAtVerticalCenter.length !== 0)
            {
                const listCenterIndex = Math.floor((itemsAtVerticalCenter.length - 1) / 2)
                return itemsAtVerticalCenter[listCenterIndex]
            }

            console.log("Centerpiece cell index not found")
            return kInvalidCellIndex
        }

        function calculateFlickableContentY(centerpieceCellIndex)
        {
            if (centerpieceCellIndex === kInvalidCellIndex)
            {
                console.log("Invalid centerpiece cell index")
                return 0
            }

            const cellWidth = centerpieceCellIndex < sizesCalculator.enlargedCellsCount
                ? sizesCalculator.enlargedCellWidth
                : sizesCalculator.normalCellWidth
            const halfCellHeight = sizesCalculator.calculateCellHeight(cellWidth / 2)

            const centerpieceCellCenterY =
                sizesCalculator.getCellY(centerpieceCellIndex) + halfCellHeight

            return Math.min(
                Math.max(0, centerpieceCellCenterY - flickable.height / 2),
                Math.max(0, flickable.contentHeight - flickable.height))
        }
    }
}