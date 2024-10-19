// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.2

import Nx.Controls 1.0
import Nx.Dialogs 1.0
import Nx.Core 1.0

DialogBase
{
    id: dialog

    property alias headerText: header.text
    property alias messageText: message.text
    property alias adviceText: advice.text

    property string certificateCommonName
    property string certificateIssuedBy
    property string certificateExpires
    property string certificateSHA256
    property string certificateSHA1

    disableAutoClose: true

    Column
    {
        width: parent.width

        DialogTitle
        {
            id: header

            wrapMode: Text.WordWrap
            visible: !!text
        }

        Item
        {
            id: holder
            width: parent.width
            height: Math.min(
                Screen.height * 0.8 - header.height - buttonBox.height,
                scrollView.implicitHeight)

            ScrollView
            {
                id: scrollView

                ScrollBar.vertical.policy: scrollView.contentHeight > scrollView.height
                    ? ScrollBar.AlwaysOn
                    : ScrollBar.AlwaysOff

                anchors.fill: parent
                clip: true

                contentHeight: column.height

                Column
                {
                    id: column

                    width: parent.width

                    DialogMessage { id: message }

                    DialogMessage
                    {
                        id: advice

                        bottomPadding: 12
                    }

                    DialogMessage
                    {
                        text: "<b>%1</b>: %2".arg(qsTr("Common name")).arg(certificateCommonName)
                        topPadding: 12
                        bottomPadding: 0
                    }

                    DialogMessage
                    {
                        text: "<b>%1</b>: %2".arg(qsTr("Issued by")).arg(certificateIssuedBy)
                        bottomPadding: 0
                    }

                    DialogMessage
                    {
                        text: "<b>%1</b>: %2".arg(qsTr("Expires")).arg(certificateExpires)
                        bottomPadding: 8
                    }

                    DialogMessage
                    {
                        text: "<b>%1</b>".arg(qsTr("Fingerprints"))
                        bottomPadding: 0
                    }

                    DialogMessage
                    {
                        text: "<b>%1</b>: %2".arg("SHA-256").arg(certificateSHA256)
                        bottomPadding: 0
                    }

                    DialogMessage
                    {
                        text: "<b>%1</b>: %2".arg("SHA-1").arg(certificateSHA1)
                    }
                }
            }

            Rectangle
            {
                id: shadow

                height: 20
                width: parent.width
                visible: scrollView.ScrollBar.vertical.position > 0

                gradient: Gradient
                {
                    GradientStop
                    {
                        position: 0.0
                        color: ColorTheme.transparent(ColorTheme.colors.dark3, 0.2)
                    }

                    GradientStop
                    {
                        position: 1.0
                        color: "transparent"
                    }
                }
            }
        }

        DialogButtonBox
        {
            id: buttonBox

            readonly property string kConnectButtonId: "CONNECT"

            width: parent.width

            buttonsModel: [{"id": "CONNECT"}, {"id": "CANCEL", "accented": true}]

            onButtonClicked:
            {
                if (buttonId == kConnectButtonId)
                    dialog.result = 'connect'

                dialog.close()
            }
        }
    }
}
