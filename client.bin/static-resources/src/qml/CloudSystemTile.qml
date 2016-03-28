import QtQuick 2.5;
import QtQuick.Controls 1.2;

import "."

BaseTile
{
    id: thisComponent;

    property string userName;

    isExpandable: false;  // TODO: change when recent systems are ready
    isAvailable: isOnline;

    centralAreaDelegate: Column
    {
        spacing: 10; // TODO: check is bottom margin is 8px

        NxLabel
        {
            id: login;

            anchors.left: parent.left;
            anchors.right: parent.right;

            anchors.leftMargin: 4;

            enabled: thisComponent.isAvailable;
            text: thisComponent.userName;
            font: Style.fonts.systemTile.info;
            standardColor: Style.colors.text;
            disabledColor: Style.colors.midlight;
        }

        Image
        {
            id: imageItem;

            anchors.left: parent.left;

            width: 24;
            height: 24;
            source: (isOnline
                ? "qrc:/skin/welcome_page/cloud_online.png"
                : "qrc:/skin/welcome_page/cloud_offline.png");
        }
    }

    MouseArea
    {
        anchors.fill: parent;

        visible: thisComponent.isOnline;
        onClicked: thisComponent.connectClicked();
    }
}
