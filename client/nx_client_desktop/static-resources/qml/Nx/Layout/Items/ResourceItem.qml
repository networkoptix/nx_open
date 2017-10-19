import QtQuick 2.6
import Nx 1.0
import Nx.Instruments 1.0
import nx.client.core 1.0
import nx.client.desktop 1.0

Item
{
    id: resourceItem

    property var layoutItemData

    property var rotationAllowed: contentItem.item && contentItem.item.rotationAllowed

    QtObject
    {
        id: d

        readonly property int kReturnToBoundsAnimationDuration: 250
        readonly property int kReturnToBoundsAnimationEasing: Easing.OutCubic
    }

    MouseArea
    {
        id: mouseArea

        hoverEnabled: true

        anchors.fill: parent
        anchors.margins: resizeInstrument.enabled ? -resizeInstrument.frameWidth / 2 : 0

        onDoubleClicked:
        {
            if (flickableView.zoomedItem !== resourceItem)
            {
                flickableView.zoomedItem = resourceItem
            }
            else if (
                Math.abs(resourceItem.width - flickableView.width)
                    < gridLayout.unzoomThreshold
                || Math.abs(resourceItem.height - flickableView.height)
                    < gridLayout.unzoomThreshold)
            {
                flickableView.zoomedItem = null
            }

            flickableView.fitInView()
        }
    }

    RotationInstrument
    {
        id: rotationInstrument

        item: mouseArea
        target: contentItem
        enabled: rotationAllowed

        onStarted: contentGeometryAnimation.complete()

        onFinished:
        {
            layoutItemData.rotation = contentItem.rotation
        }
    }

    ResizeInstrument
    {
        id: resizeInstrument

        item: mouseArea
        target: contentItem
        cursorManager: cursorManager

        frameWidth: 4 / contentItem.width * resourceItem.width
        minWidth: gridLayout.cellSize.width / 2
        minHeight: gridLayout.cellSize.height / 3

        onStarted:
        {
            contentGeometryAnimation.complete()
            resourceItem.parent.z = 1
        }

        onFinished: contentGeometryAnimation.start()
    }

    DragInstrument
    {
        id: dragInstrument

        item: mouseArea
        target: contentItem

        onStarted:
        {
            contentGeometryAnimation.stop()
            resourceItem.parent.z = 1
        }

        onFinished: contentGeometryAnimation.start()
    }

    Item
    {
        id: cursorHolder
        anchors.fill: contentItem
        anchors.margins: resizeInstrument.resizing ? -64 : -resizeInstrument.frameWidth
    }

    CursorManager
    {
        id: cursorManager
        target: cursorHolder
    }

    Loader
    {
        id: contentItem

        sourceComponent:
        {
            if (!layoutItemData.resource)
                return

            if (layoutItemData.resource.flags & NxGlobals.MediaResourceFlag)
                return mediaItemComponent

            if (layoutItemData.resource.flags & NxGlobals.ServerResourceFlag)
                return serverItemComponent

            if (layoutItemData.resource.flags & NxGlobals.WebPageResourceFlag)
                return webPageItemComponent
        }

        property rect enclosedGeometry: Geometry.encloseRotatedGeometry(
            Qt.rect(0, 0, parent.width, parent.height),
            Geometry.aspectRatio(Qt.size(parent.width, parent.height)),
            rotation)

        readonly property bool enableGeometryBindings:
            !contentGeometryAnimation.running
            && !resizeInstrument.resizing

        Binding
        {
            target: contentItem
            property: "x"
            value: (resourceItem.width - contentItem.enclosedGeometry.width) / 2
            when: contentItem.enableGeometryBindings
        }
        Binding
        {
            target: contentItem
            property: "y"
            value: (resourceItem.height - contentItem.enclosedGeometry.height) / 2
            when: contentItem.enableGeometryBindings
        }
        Binding
        {
            target: contentItem
            property: "width"
            value: contentItem.enclosedGeometry.width
            when: contentItem.enableGeometryBindings
        }
        Binding
        {
            target: contentItem
            property: "height"
            value: contentItem.enclosedGeometry.height
            when: contentItem.enableGeometryBindings
        }
        Binding
        {
            target: contentItem
            property: "rotation"
            value: layoutItemData.rotation
            when: !rotationInstrument.rotating
        }

        ParallelAnimation
        {
            id: contentGeometryAnimation

            NumberAnimation
            {
                target: contentItem
                property: "x"
                to: (resourceItem.width - contentItem.enclosedGeometry.width) / 2
                duration: d.kReturnToBoundsAnimationDuration
                easing.type: d.kReturnToBoundsAnimationEasing
            }
            NumberAnimation
            {
                target: contentItem
                property: "y"
                to: (resourceItem.height - contentItem.enclosedGeometry.height) / 2
                duration: d.kReturnToBoundsAnimationDuration
                easing.type: d.kReturnToBoundsAnimationEasing
            }
            NumberAnimation
            {
                target: contentItem
                property: "width"
                to: contentItem.enclosedGeometry.width
                duration: d.kReturnToBoundsAnimationDuration
                easing.type: d.kReturnToBoundsAnimationEasing
            }
            NumberAnimation
            {
                target: contentItem
                property: "height"
                to: contentItem.enclosedGeometry.height
                duration: d.kReturnToBoundsAnimationDuration
                easing.type: d.kReturnToBoundsAnimationEasing
            }

            onStopped: resourceItem.parent.z = 0
        }
    }

    Component
    {
        id: mediaItemComponent
        MediaItemDelegate
        {
            layoutItemData: resourceItem.layoutItemData
        }
    }
    Component
    {
        id: serverItemComponent
        ServerItemDelegate
        {
            layoutItemData: resourceItem.layoutItemData
        }
    }
    Component
    {
        id: webPageItemComponent
        WebPageItemDelegate
        {
            layoutItemData: resourceItem.layoutItemData
        }
    }
}
