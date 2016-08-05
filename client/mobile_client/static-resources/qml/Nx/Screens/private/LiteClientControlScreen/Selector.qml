import QtQuick 2.6

Item
{
    property alias currentResourceId: camerasStripe.currentResourceId
    property alias currentThumbnail: camerasStripe.currentThumbnail
    property alias fourCamerasMode: fourCamerasModeButton.checked

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
            checkedImage: lp("/images/screen_mode_1_selected.png")
            checkable: true
            checked: true
            autoExclusive: true
        }
        ImageButton
        {
            id: fourCamerasModeButton
            image: lp("/images/screen_mode_4.png")
            checkedImage: lp("/images/screen_mode_4_selected.png")
            checkable: true
            autoExclusive: true
        }
    }
}
