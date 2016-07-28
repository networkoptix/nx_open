import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

ListView
{
    id: camerasStripe

    property string currentResourceId: ""
    property string currentThumbnail: ""

    implicitHeight: 88
    topMargin: 1
    bottomMargin: 1
    leftMargin: 8
    rightMargin: 8
    orientation: Qt.Horizontal

    readonly property real cellHeight: height - topMargin - bottomMargin
    readonly property real cellWidth: Math.floor(cellHeight * 16 / 9) + spacing

    currentIndex: -1

    delegate: CameraDisplay
    {
        width: cellWidth
        height: cellHeight
        resourceId: model.uuid
        thumbnail: model.thumbnail
        status: model.resourceStatus

        onClicked:
        {
            currentResourceId = model.uuid
            currentThumbnail = model.thumbnail
        }
    }

    header: NoCameraItem
    {
        width: cellWidth + 16
        height: cellHeight
        active: currentIndex == -1

        onClicked:
        {
            currentResourceId = ""
            currentThumbnail = ""
        }
    }

    highlightFollowsCurrentItem: false
    highlight: Rectangle
    {
        anchors.fill: currentItem
        anchors.margins: -1

        color: "transparent"
        border.color: ColorTheme.brand_main
        border.width: 2
        radius: 2
    }

    model: QnCameraListModel {}

    onCurrentResourceIdChanged:
    {
        currentIndex = model.rowByResourceId(currentResourceId)
        if (currentIndex < 0)
        {
            contentX = headerItem.x - leftPadding
        }
    }

    ScrollIndicator.horizontal: ScrollIndicator {}
}
