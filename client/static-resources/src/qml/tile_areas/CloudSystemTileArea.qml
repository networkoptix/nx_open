import QtQuick 2.6;

import ".."

Column
{
    id: control;

    property string userName;
    property bool isOnline;

    spacing: 10; // TODO: check is bottom margin is 8px

    NxLabel
    {
       id: login;

       anchors.left: parent.left;
       anchors.right: parent.right;

       anchors.leftMargin: 4;

       disableable: false;
       enabled: control.isOnline;
       text: control.userName;
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
