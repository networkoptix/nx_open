// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Ui

BaseSettingsPage
{
    id: appInfoPage

    signal developerSettingsRequested

    rightControl: ClipboardCopyButton
    {
        id: clipboardCopyButton

        textToCopy: `${companyNameText.text} ${versionText.text}`
    }

    title: qsTr("App Info")

    Rectangle
    {
        width: parent.width
        height: contentColumn.height
        color: ColorTheme.colors.dark6
        radius: LayoutController.isTablet ? 8 : 0

        Column
        {
            id: contentColumn
            spacing: 8

            width: parent.width

            topPadding: 18
            bottomPadding: 18

            Image
            {
                anchors.horizontalCenter: parent.horizontalCenter

                source: "image://skin/logo.png"
                width: 64
                height: width

                MouseArea
                {
                    id: developerSettingsClickArea

                    property int clicksCount: 0

                    anchors.fill: parent

                    onClicksCountChanged:
                    {
                        if (clicksCount < 5)
                            return

                        clicksCount = 0

                        appInfoPage.developerSettingsRequested()
                    }

                    onClicked:
                    {
                        ++clicksCount
                        clicksResetTimer.restart()
                    }

                    Timer
                    {
                        id: clicksResetTimer

                        interval: 1000
                        onTriggered: developerSettingsClickArea.clicksCount = 0
                    }
                }
            }

            Text
            {
                id: companyNameText

                anchors.horizontalCenter: parent.horizontalCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 18
                font.weight: 500
                lineHeight: 1.25
                color: ColorTheme.colors.light4
                text: appContext.appInfo.companyName();
            }

            Text
            {
                id: versionText

                anchors.horizontalCenter: parent.horizontalCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 14
                font.weight: 400
                lineHeight: 1.5
                color: ColorTheme.colors.light16
                text: appContext.appInfo.version();
            }
        }
    }
}
