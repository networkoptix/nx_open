// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

Control
{
    id: contextHintButton

    property int helpTopic: 0
    property string toolTipText: "Sample context hint"

    baselineOffset: image.baselineOffset + topPadding

    contentItem: SvgImage
    {
        id: image

        primaryColor: hasHelpTopic && hovered && !mouseArea.containsPress
            ? "dark15"
            : "dark14"

        sourcePath: "skin/buttons/context_hint_16.svg"
        sourceSize: Qt.size(16, 16)

        baselineOffset: 12.5
        opacity: enabled ? 1.0 : 0.4

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton

            GlobalToolTip.delay: 0
            GlobalToolTip.text:
            {
                var text = "<p>" + toolTipText + "</p>"
                if (hasHelpTopic)
                {
                    text += "<i style='color: " + ColorTheme.colors.light16 + "'>"
                        + qsTr("Click on the icon to read more") + "</i>"
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
        helpTopic != HelpTopic.Empty && helpTopic != HelpTopic.ForcedEmpty
}
