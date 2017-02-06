import QtQuick 2.6;

import ".."

Column
{
    id: control;

    property string userName;
    property bool isConnectable: false;

    spacing: 10; // TODO: check is bottom margin is 8px

    NxLabel
    {
        id: login;

        anchors.left: parent.left;
        anchors.right: parent.right;

        anchors.leftMargin: 4;

        disableable: false;
        enabled: control.enabled;
        text: control.userName;
        font: Style.fonts.systemTile.info;
        standardColor: Style.colors.text;
        disabledColor: Style.colors.midlight;
    }

    Image
    {
        id: imageItem;
        enabled: control.enabled;

        anchors.left: parent.left;

        width: 24;
        height: 24;
        source:
        {
           if (!context.isCloudEnabled)
               return "qrc:/skin/cloud/cloud_20_offline_disabled.png";

           return (control.isConnectable
            ? "qrc:/skin/cloud/cloud_20_accented.png"
            : "qrc:/skin/cloud/cloud_20_disabled.png");
        }
    }
}
