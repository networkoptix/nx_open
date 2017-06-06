import QtQuick 2.6
import Nx.Dialogs 1.0
import Nx.Media 1.0

DialogListItem
{
    id: control

    property int quality: 0

    readonly property bool customQuality: quality >= MediaPlayer.CustomVideoQuality

    enabled: !customQuality || qualityDialog.availableCustomQualities.indexOf(quality) !== -1
    active: qualityDialog.activeQuality == quality
    text: quality + 'p'

    onClicked:
    {
        qualityDialog.activeQuality = quality
        qualityDialog.close()
    }

    onActiveChanged:
    {
        if (active)
            qualityDialog.activeItem = this
    }
}
