import QtQuick 2.4;
Item
{
    id: thisComponent;

    function toggle() { expanded = !expanded; }

    property Component headerDelegate;
    property Component areaDelegate;

    property bool expanded: true;
    property alias area: areaDelegateLoader.item;

    height: (headerDelegateLoader.height + (expanded ? areaDelegateLoader.height : 0));

    Loader
    {
        id: headerDelegateLoader;

        height: (headerDelegate ? item.height : 0);
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
        }

        sourceComponent: headerDelegate;
    }

    Loader
    {
        id: areaDelegateLoader;

        height: (areaDelegate ? item.height : 0);
        
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: headerDelegateLoader.bottom;
        }

        visible: expanded;
        sourceComponent: areaDelegate;
    }
}
