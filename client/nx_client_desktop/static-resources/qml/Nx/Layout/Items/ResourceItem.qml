import QtQuick 2.6
import Nx 1.0
import Nx.Instruments 1.0

Item
{
    id: resourceItem

    property var modelData

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

    ResizeInstrument
    {
        id: resizeInstrument

        item: mouseArea
        target: contentItem
    }

    DragInstrument
    {
        id: rotationInstrument

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

        x: 0
        y: 0
        width: parent.width
        height: parent.height

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

        NumberAnimation
        {
            id: contentPositionAnimation
            target: contentItem
            properties: "x,y"
            to: 0
            duration: 250
            easing.type: Easing.OutCubic
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
