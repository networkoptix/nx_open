import QtQuick 2.5;

Item
{
    id: thisComponent;

    property bool isMasked: false;
    property Component maskedAreaDelegate;
    property Component areaDelegate;

    property alias area: areaLoader.item;

    MouseArea
    {
        anchors.fill: areaLoader;
        onClicked: { thisComponent.isMasked = !thisComponent.isMasked; }
    }

    Loader
    {
        id: areaLoader;

        sourceComponent: (thisComponent.isMasked
            ? thisComponent.maskedAreaDelegate
            : thisComponent.areaDelegate);
    }
}
