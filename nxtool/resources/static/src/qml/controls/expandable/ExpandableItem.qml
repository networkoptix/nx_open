import QtQuick 2.1;
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
