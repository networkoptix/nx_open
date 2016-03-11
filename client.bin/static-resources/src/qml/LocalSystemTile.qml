import QtQuick 2.5;
import QtQuick.Controls 1.2;

BaseTile
{
    id: thisComponent;

    property string host;
    property string userName;
    property bool isComaptible;

    property QtObject activeItemSelector: SingleActiveItemSelector
    {
        variableName: "isMasked";
        onIsSomeoneActiveChanged:
        {
            if (isSomeoneActive)
                thisComponent.isExpanded = true;
        }
    }

    onIsExpandedChanged:
    {
        if (!isExpanded)
            activeItemSelector.resetCurrentItem();
    }

    centralAreaDelegate: Column
    {
        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        InfoItem
        {
            text: thisComponent.host;
            iconUrl: (isComaptible ? "non_empty_url" : "");// TODO: change to proper url

            Component.onCompleted: activeItemSelector.addItem(this);    // TODO: add ActiveWatchable qml component?
        }

        InfoItem
        {
            text: thisComponent.userName;

            Component.onCompleted: activeItemSelector.addItem(this);
        }
    }

    expandedAreaDelegate: Column
    {
        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);

        Button
        {
            anchors.left: parent.left;
            anchors.right: parent.right;

            text: qsTr("Connect");
        }
    }
}











