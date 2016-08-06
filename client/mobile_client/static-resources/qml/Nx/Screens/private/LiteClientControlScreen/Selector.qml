import QtQuick 2.6

Item
{
    property alias currentResourceId: camerasStripe.currentResourceId
    property alias currentThumbnail: camerasStripe.currentThumbnail
    property bool fourCamerasMode: false

    CamerasStripe
    {
        id: camerasStripe
        width: parent.width
    }

    Row
    {
        id: modeSeceltor

        anchors.bottom: parent.bottom
        padding: 16
        x: -8

        ImageButton
        {
            id: singleCameraModeButton
            image: lp("/images/screen_mode_1.png")
            selectedImage: lp("/images/screen_mode_1_selected.png")
            selected: true
            onClicked: fourCamerasMode = false
        }
        ImageButton
        {
            id: fourCamerasModeButton
            image: lp("/images/screen_mode_4.png")
            selectedImage: lp("/images/screen_mode_4_selected.png")
            onClicked: fourCamerasMode = true
        }
    }

    onFourCamerasModeChanged:
    {
        singleCameraModeButton.selected = !fourCamerasMode
        fourCamerasModeButton.selected = fourCamerasMode
    }
}
