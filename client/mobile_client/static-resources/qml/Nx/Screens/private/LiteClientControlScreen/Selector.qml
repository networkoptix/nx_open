import QtQuick 2.6

Item
{
    id: selector

    property alias currentResourceId: camerasStripe.currentResourceId
    property bool fourCamerasMode: true

    signal singleCameraModeClicked()
    signal multipleCmaerasModeClicked()
    signal cameraClicked(string resourceId)

    CamerasStripe
    {
        id: camerasStripe
        width: parent.width
        onCameraClicked: selector.cameraClicked(resourceId)
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
            autoExclusive: true
            onClicked: singleCameraModeClicked()
        }
        ImageButton
        {
            id: fourCamerasModeButton
            image: lp("/images/screen_mode_4.png")
            checkedImage: lp("/images/screen_mode_4_selected.png")
            checkable: true
            checked: true
            autoExclusive: true
            onClicked: multipleCmaerasModeClicked()
        }
    }

    onFourCamerasModeChanged:
    {
        singleCameraModeButton.checked = !fourCamerasMode
        fourCamerasModeButton.checked = fourCamerasMode
    }
}
