import QtQuick 2.5;

Item
{
    id: thisComponent;

    property string value;

    property bool isMasked: isAvailable && isMaskedPrivate;
    property bool isAvailable: true;
    property bool isMaskedPrivate;

    property Component maskedAreaDelegate;
    property Component areaDelegate;

    property alias area: areaLoader.item;

    MouseArea
    {
        anchors.fill: areaLoader;
        onClicked: { thisComponent.isMaskedPrivate = !thisComponent.isMaskedPrivate; }
    }

    Loader
    {
        id: areaLoader;

        sourceComponent: (thisComponent.isMasked
            ? thisComponent.maskedAreaDelegate
            : thisComponent.areaDelegate);
    }
}
