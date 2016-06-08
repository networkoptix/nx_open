import QtQuick 2.6
import QtQuick.Layouts 1.1
import Nx 1.0

Item
{
    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    property alias description: descriptionText.text
    property bool online: true

    Column
    {

        spacing: 4

        Text
        {
            id: descriptionText

            width: parent.width
            font.pixelSize: 14
            elide: Text.ElideRight
            color: enabled ? ColorTheme.windowText : ColorTheme.base13
        }

        Image
        {
            source: enabled ? lp("/images/cloud.png")
                            : lp("/images/cloud_disabled.png")
        }
    }

    IssueLabel
    {
        text: online ? "" : qsTr("OFFLINE")
        anchors
        {
            bottom: parent.bottom
            right: parent.right
            rightMargin: -4
        }
        color: ColorTheme.base14
        visible: online
    }
}

