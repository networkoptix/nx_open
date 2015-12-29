import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"

Rectangle {

    property string systemName
    property string host
    property int port
    property string version

    implicitWidth: parent ? parent.width : dp(200)
    implicitHeight: isCompatible ? dp(79) : dp(103)
    color: isCompatible ? QnTheme.sessionItemBackground : QnTheme.sessionItemBackgroundIncompatible
    radius: dp(2)

    QnMaterialSurface {
        onClicked: {
            if (isCompatible)
                Main.openDiscoveredSession(host, port, systemName)
            else
                oldClientOfferDialog.show()
        }
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        x: dp(16)
        width: parent.width - 2 * x

        Text {
            text: systemName ? systemName : "<%1>".arg(qsTr("Unknown"))
            color: isCompatible ? QnTheme.listText : QnTheme.listDisabledText
            font.pixelSize: sp(18)
            font.weight: Font.DemiBold
            height: dp(32)
            verticalAlignment: Text.AlignVCenter
            width: parent.width - dp(32)
            elide: Text.ElideRight
        }

        Text {
            text: host + ":" + port
            color: isCompatible ? QnTheme.listSubText : QnTheme.listDisabledText
            font.pixelSize: sp(15)
            font.weight: Font.DemiBold
            height: dp(24)
            verticalAlignment: Text.AlignVCenter
            width: parent.width - dp(32)
            elide: Text.ElideRight
        }

        Text {
            text: {
                var res = qsTr("incompatible server version")
                if (version) {
                    res += ": "
                    res += version
                }
                return res
            }
            color: QnTheme.sessionItemIncompatibleMark
            font.pixelSize: sp(12)
            font.weight: Font.DemiBold
            height: dp(24)
            verticalAlignment: Text.AlignVCenter
            visible: !isCompatible
        }
    }
}
