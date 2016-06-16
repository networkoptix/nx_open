import QtQuick 2.6
import QtQuick.Layouts 1.3
import Nx 1.0

SideNavigationItem
{
    property alias text: label.text
    property string resourceId
    property bool shared: false
    property int count: 0

    implicitHeight: 40

    RowLayout
    {
        anchors.fill: parent
        spacing: 12
        anchors.leftMargin: 12
        anchors.rightMargin: 16

        Image
        {
            Layout.alignment: Qt.AlignVCenter
            source:
            {
                var icon = undefined
                if (!resourceId)
                    icon = "all_cameras"
                else if (shared)
                    icon = "global_layout"
                else
                    icon = "layout"

                if (active)
                    icon += "_active"

                return lp("/images/" + icon + ".png")
            }
        }

        Text
        {
            id: label

            Layout.fillWidth: true
            Layout.fillHeight: true

            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            color: active ? ColorTheme.windowText : ColorTheme.contrast10
        }

        Text
        {
            id: countLabel

            Layout.alignment: Qt.AlignVCenter

            text: count
            font.pixelSize: 15
            color: active ? ColorTheme.contrast16 : ColorTheme.base16
        }
    }
}
