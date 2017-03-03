import Nx.Dialogs 1.0

DialogListItem
{
    id: control

    property int quality: 0

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
