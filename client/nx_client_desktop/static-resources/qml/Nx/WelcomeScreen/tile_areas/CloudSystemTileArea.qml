import QtQuick 2.6;
import Nx 1.0;

import ".."

Column
{
    id: control;

    property string userName;
    property bool isConnectable: false;
    property bool isAvailable: false
    property alias indicators: indicatorsRow

    spacing: 14; // TODO: check is bottom margin is 8px

    NxLabel
    {
        id: login;

        anchors.left: parent.left;
        anchors.right: parent.right;

        anchors.leftMargin: 4;
        elide: Text.ElideRight;

        disableable: false;
        enabled: control.isAvailable;
        text: control.userName;
        font.pixelSize: 12;
        standardColor: ColorTheme.text;
        disabledColor: ColorTheme.midlight;
    }

    Item
    {
        width: parent.width
        height: imageItem.height

        Image
        {
            id: imageItem;
            enabled: control.isAvailable;

            anchors.left: parent.left;

            width: 20;
            height: 20;
            source:
            {
               if (!context.isCloudEnabled)
                   return "qrc:/skin/cloud/cloud_20_offline_disabled.png";

               return (control.isConnectable
                ? "qrc:/skin/cloud/cloud_20_accented.png"
                : "qrc:/skin/cloud/cloud_20_disabled.png");
            }
        }

        IndicatorsRow
        {
            id: indicatorsRow

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            maxWidth: parent.width - (imageItem.x + imageItem.width + 14)
        }
    }
}
