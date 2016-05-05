import QtQuick 2.5;

Item
{
    id: thisComponent;

    property string value;

    property bool isMasked: isAvailable && isMaskedPrivate;
    property bool isAvailable: true;
    property bool isMaskedPrivate: false;

    property Component maskedAreaDelegate;
    property Component areaDelegate;

    property var area: (isMasked ? maskedAreaLoader.item : areaLoader.item);
    property var maskedArea: maskedAreaLoader.item;

    height: area.height;

    MouseArea
    {
        anchors.fill: (isMasked ? maskedAreaLoader : areaLoader);
        onClicked: { thisComponent.isMaskedPrivate = !thisComponent.isMaskedPrivate; }
        acceptedButtons: (isAvailable ? Qt.AllButtons : Qt.NoButton);
    }

    Loader
    {
        id: maskedAreaLoader;

        visible: isMasked;
        sourceComponent: thisComponent.maskedAreaDelegate;
    }

    Loader
    {
        id: areaLoader;

        visible: !isMasked;
        sourceComponent: thisComponent.areaDelegate;
    }
}
