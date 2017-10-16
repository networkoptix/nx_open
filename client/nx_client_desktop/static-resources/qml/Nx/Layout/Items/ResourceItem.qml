import QtQuick 2.6
import Nx 1.0
import Nx.Instruments 1.0
import nx.client.core 1.0

Item
{
    id: resourceItem

    property var modelData

    property var rotationAllowed: contentItem.item && contentItem.item.rotationAllowed

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

        onStarted:
        {
            contentPositionAnimation.complete()
        }
    }

    ResizeInstrument
    {
        id: resizeInstrument

        item: mouseArea
        target: contentItem

        onStarted:
        {
            contentPositionAnimation.complete()
        }
    }

    DragInstrument
    {
        id: dragInstrument

        item: mouseArea
        target: contentItem

        onStarted:
        {
            contentPositionAnimation.stop()
            resourceItem.parent.z = 1
        }

        onFinished:
        {
            contentPositionAnimation.start()
        }
    }

    Loader
    {
        id: contentItem

        sourceComponent:
        {
            if (!modelData.resource)
                return

            if (modelData.resource.flags & NxGlobals.MediaResourceFlag)
                return mediaItemComponent

            if (modelData.resource.flags & NxGlobals.ServerResourceFlag)
                return serverItemComponent

            if (modelData.resource.flags & NxGlobals.WebPageResourceFlag)
                return webPageItemComponent
        }

        property rect enclosedGeometry: Geometry.encloseRotatedGeometry(
            Qt.rect(0, 0, parent.width, parent.height),
            Geometry.aspectRatio(Qt.size(parent.width, parent.height)),
            rotation)

        width: enclosedGeometry.width
        height: enclosedGeometry.height

        Binding
        {
            target: contentItem
            property: "x"
            value: (resourceItem.width - contentItem.width) / 2
            when: !contentPositionAnimation.running
        }
        Binding
        {
            target: contentItem
            property: "y"
            value: (resourceItem.height - contentItem.height) / 2
            when: !contentPositionAnimation.running
        }

        ParallelAnimation
        {
            id: contentPositionAnimation

            NumberAnimation
            {
                target: contentItem
                property: "x"
                to: (resourceItem.width - contentItem.width) / 2
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation
            {
                target: contentItem
                property: "y"
                to: (resourceItem.height - contentItem.height) / 2
                duration: 250
                easing.type: Easing.OutCubic
            }

            onStopped: resourceItem.parent.z = 0
        }
    }

    Component
    {
        id: mediaItemComponent
        MediaItemDelegate
        {
            modelData: resourceItem.modelData
        }
    }
    Component
    {
        id: serverItemComponent
        ServerItemDelegate
        {
            modelData: resourceItem.modelData
        }
    }
    Component
    {
        id: webPageItemComponent
        WebPageItemDelegate
        {
            modelData: resourceItem.modelData
        }
    }
}
