import QtQuick 2.6
import Nx 1.0
import Nx.Instruments 1.0
import nx.client.core 1.0
import nx.client.desktop 1.0

Item
{
    id: resourceItemControl

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
            if (flickableView.zoomedItem !== resourceItemControl)
            {
                flickableView.zoomedItem = resourceItemControl
            }
            else if (
                Math.abs(resourceItemControl.width - flickableView.width)
                    < gridLayout.unzoomThreshold
                || Math.abs(resourceItemControl.height - flickableView.height)
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
    readonly property alias rotationInstrument: rotationInstrument

    ResizeInstrument
    {
        id: resizeInstrument

        item: mouseArea
        target: contentItem
        cursorManager: cursorManager

        frameWidth: 4 / contentItem.width * resourceItemControl.width
        minWidth: gridLayout.cellSize.width / 2
        minHeight: gridLayout.cellSize.height / 3

        onStarted:
        {
            contentGeometryAnimation.complete()
            resourceItemControl.parent.z = 1
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
            resourceItemControl.parent.z = 1
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

        property rect enclosedGeometry: parent.width === 0 || parent.height === 0
            ? Qt.rect(0, 0, 0, 0)
            : Geometry.encloseRotatedGeometry(
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
            value: (resourceItemControl.width - contentItem.enclosedGeometry.width) / 2
            when: contentItem.enableGeometryBindings
        }
        Binding
        {
            target: contentItem
            property: "y"
            value: (resourceItemControl.height - contentItem.enclosedGeometry.height) / 2
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
                to: (resourceItemControl.width - contentItem.enclosedGeometry.width) / 2
                duration: d.kReturnToBoundsAnimationDuration
                easing.type: d.kReturnToBoundsAnimationEasing
            }
            NumberAnimation
            {
                target: contentItem
                property: "y"
                to: (resourceItemControl.height - contentItem.enclosedGeometry.height) / 2
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

            onStopped: resourceItemControl.parent.z = 0
        }

        Behavior on rotation
        {
            enabled: !rotationInstrument.rotating
            NumberAnimation { duration: 200 }
        }
    }

    Component
    {
        id: mediaItemComponent
        MediaItemDelegate
        {
            resourceItem: resourceItemControl
            layoutItemData: resourceItemControl.layoutItemData
        }
    }
    Component
    {
        id: serverItemComponent
        ServerItemDelegate
        {
            resourceItem: resourceItemControl
            layoutItemData: resourceItemControl.layoutItemData
        }
    }
    Component
    {
        id: webPageItemComponent
        WebPageItemDelegate
        {
            resourceItem: resourceItemControl
            layoutItemData: resourceItemControl.layoutItemData
        }
    }
}
