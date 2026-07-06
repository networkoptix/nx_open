// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Controls

import Nx.Core
import Nx.Mobile.Controls
import Nx.Mobile.Ui.Sheets
import Nx.Ui

import nx.vms.client.mobile

Action
{
    id: action

    readonly property bool shared: backend.isShared && !analyticsMode
    property bool analyticsMode: false
    property alias objectData: backend.objectData
    readonly property alias backend: backend

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
            shareBookmarkSheet.open()
    }

    readonly property NxObject d: NxObject
    {
        ShareBookmarkBackend
        {
            id: backend

            onBookmarkCreated: Workflow.showBanner(qsTr("Bookmark created"), Banner.Success)
            onSharingFailed: Workflow.showBanner(qsTr("Cannot share bookmark"), Banner.Error)
        }

        SheetLoader
        {
            id: shareBookmarkSheet

            ShareBookmarkSheet
            {
                backend: action.backend
                isAnalyticsItemMode: action.analyticsMode

                onShowHowItWorks: howItWorksSheet.open()
            }
        }

        SheetLoader
        {
            id: howItWorksSheet

            HowItWorksSheet
            {
                description: qsTr("Sharing opens the new bookmark dialog to generate a playback"
                    + " link after setting the sharing options")

                doNotShowAgain: !appContext.settings.showHowShareWorksNotification

                onContinued:
                {
                    appContext.settings.showHowShareWorksNotification = !doNotShowAgain
                    shareBookmarkSheet.open()
                }
            }
        }
    }
}
