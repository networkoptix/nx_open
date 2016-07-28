import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

SideNavigationItem
{
    id: sideNavigationItem

    property string text
    property string resourceId
    property bool shared: false
    property int count: 0
    property int type: QnLayoutsModel.Layout

    implicitHeight: 40

    Row
    {
        anchors.fill: parent
        spacing: 12
        anchors.leftMargin: 12
        anchors.rightMargin: 16

        Image
        {
            id: icon

            anchors.verticalCenter: parent.verticalCenter
            source:
            {
                var icon = undefined

                if (type == QnLayoutsModel.AllCameras)
                    icon = "all_cameras"
                else if (type == QnLayoutsModel.Layout)
                    icon = shared ? "global_layout" : "layout"
                else if (type == QnLayoutsModel.LiteClient)
                    icon = "lite_client"

                if (active)
                    icon += "_active"

                return lp("/images/" + icon + ".png")
            }
        }

        Row
        {
            width: parent.width - icon.width - countLabel.width - 2 * parent.spacing
            height: parent.height
            spacing: 4

            Text
            {
                id: label

                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, parent.width)

                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                color: active ? ColorTheme.windowText : ColorTheme.contrast10

                text: type == QnLayoutsModel.LiteClient ? "Nx1" : sideNavigationItem.text
            }

            Text
            {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - label.width - parent.spacing

                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 15
                elide: Text.ElideRight
                color: active ? ColorTheme.contrast10 : ColorTheme.contrast16

                text: "(" + (type == QnLayoutsModel.LiteClient ? sideNavigationItem.text : "") + ")"
                visible: type == QnLayoutsModel.LiteClient
            }
        }

        Text
        {
            id: countLabel

            anchors.verticalCenter: parent.verticalCenter

            text: count
            font.pixelSize: 15
            color: active ? ColorTheme.contrast16 : ColorTheme.base16
            visible: type != QnLayoutsModel.LiteClient
        }
    }
}
