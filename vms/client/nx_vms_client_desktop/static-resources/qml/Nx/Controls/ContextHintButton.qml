import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx 1.0

import nx.vms.client.desktop 1.0

Control
{
    id: contextHintButton

    property int helpTopic: 0
    property string toolTipText: "Sample context hint"

    baselineOffset: image.baselineOffset + topPadding

    contentItem: Image
    {
        id: image

        source: hasHelpTopic && hovered && !mouseArea.containsPress
            ? "qrc:///skin/buttons/context_info_hovered.png"
            : "qrc:///skin/buttons/context_info.png"

        baselineOffset: 12.5
        opacity: enabled ? 1.0 : 0.4

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            enabled: hasHelpTopic

            GlobalToolTip.delay: 0
            GlobalToolTip.text:
            {
                var text = "<p>" + toolTipText + "</p>"
                if (hasHelpTopic)
                {
                    text += "<i style='color: " + ColorTheme.colors.light16 + "'>"
                        + qsTr("Click to read more") + "</i>"
                }

                return "<html>" + text + "</html>"
            }

            onClicked:
            {
                if (hasHelpTopic)
                    HelpHandler.openHelpTopic(helpTopic)
            }
        }
    }

    readonly property bool hasHelpTopic:
    {
        const kEmptyHelp = 0
        const kForcedEmptyHelp = 1
        return helpTopic != kEmptyHelp && helpTopic != kForcedEmptyHelp
    }
}
