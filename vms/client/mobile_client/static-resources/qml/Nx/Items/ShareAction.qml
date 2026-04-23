// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Controls

import Nx.Core
import Nx.Mobile.Ui.Sheets
import Nx.Ui
import nx.vms.client.mobile

Action
{
    readonly property bool shared: backend.isShared && !analyticsMode
    property alias analyticsMode: shareBookmarkSheet.isAnalyticsItemMode
    property alias objectData: backend.objectData

    text: shared ? qsTr("Shared") : qsTr("Share")

    icon.source: shared
        ? `image://skin/20x20/Solid/shared.svg?primary=light10&secondary=green`
        : `image://skin/20x20/Solid/share.svg?primary=light10`

    enabled: backend.isAvailable

    onTriggered: share()

    function share()
    {
        if (!backend.isAvailable)
            return

        if (appContext.settings.showHowShareWorksNotification && analyticsMode)
            howItWorksSheet.open()
        else
            shareBookmarkSheet.showSheet()
    }

    readonly property NxObject d: NxObject
    {
        ShareBookmarkBackend
        {
            id: backend

            onErrorOccurred: Workflow.openStandardPopup(title, text)
        }

        ShareBookmarkSheet
        {
            id: shareBookmarkSheet

            backend: backend

            onShowHowItWorks: howItWorksSheet.open()
        }

        HowItWorksSheet
        {
            id: howItWorksSheet

            description: qsTr("Sharing opens the new bookmark dialog to generate a playback link" +
                " after setting the sharing options")

            doNotShowAgain: !appContext.settings.showHowShareWorksNotification

            onContinued:
            {
                appContext.settings.showHowShareWorksNotification = !doNotShowAgain
                shareBookmarkSheet.showSheet()
            }
        }
    }
}
