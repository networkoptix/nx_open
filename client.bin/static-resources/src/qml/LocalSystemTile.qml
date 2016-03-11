import QtQuick 2.5;

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
        anchors.top: (parent ? parent.top : undefined);
        anchors.left: (parent ? parent.left : undefined);
        anchors.right: (parent ? parent.right : undefined);
        anchors.bottom: (parent ? parent.bottom : undefined);

        anchors.topMargin: 12;
        anchors.bottomMargin: 16;

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
}
