import QtQuick 2.0;

FocusScope
{
    id: thisComponent;

    property bool showFirst: true;

    property Component first;
    property Component second;
    property alias currentItem: loader.item;
    
    width: (loader.item ? loader.item.width : 0)
    height: (loader.item ? loader.item.height : 0)

    Loader
    {
        id: loader;

        sourceComponent: (showFirst ? first : second);
    }
}
