import QtQuick 2.6
import Nx 1.0

Item
{
    id: resourceItem

    property var modelData

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent

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

    Loader
    {
        anchors.fill: parent

        sourceComponent:
        {
            if (!modelData.resource)
                return

            if (modelData.resource.flags & Nx.MediaResourceFlag)
                return mediaItemComponent

            if (modelData.resource.flags & Nx.ServerResourceFlag)
                return serverItemComponent

            if (modelData.resource.flags & Nx.WebPageResourceFlag)
                return webPageItemComponent
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
