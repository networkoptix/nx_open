import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

ListView
{
    id: camerasStripe

    property string currentResourceId: ""

    signal cameraClicked(string resourceId)

    implicitHeight: 88
    topMargin: 1
    bottomMargin: 1
    leftMargin: 8
    rightMargin: 8
    orientation: Qt.Horizontal

    readonly property real cellHeight: height - topMargin - bottomMargin

    currentIndex: -1

    delegate: CameraDisplay
    {
        implicitHeight: cellHeight
        resourceId: model.uuid
        thumbnail: model.thumbnail
        status: model.resourceStatus

        onClicked: camerasStripe.cameraClicked(model.uuid)
        onThumbnailRefreshRequested: camerasStripe.model.refreshThumbnail(index)
    }

    header: NoCameraItem
    {
        width: Math.floor(cellHeight * 16 / 9) + 16
        height: cellHeight
        active: currentIndex == -1

        onClicked: cameraClicked("")
    }

    highlight: Item
    {
        Rectangle
        {
            anchors.fill: parent
            anchors.margins: -1
            color: "transparent"
            border.color: ColorTheme.brand_main
            border.width: 2
            radius: 2
        }
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
