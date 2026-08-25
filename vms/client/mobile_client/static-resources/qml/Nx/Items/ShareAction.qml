// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Controls

import Nx.Core
import Nx.Mobile.Controls
import Nx.Mobile.Ui.Sheets
import Nx.Ui

import nx.vms.client.mobile
import nx.vms.client.mobile.timeline as Timeline

Action
{
    id: action

    required property int objectsType
    property alias objectData: backend.objectData
    readonly property alias backend: backend
    property int preferredSheetEdge: Qt.RightEdge

    // True while a spawned sheet is visible, so callers can suspend conflicting background gestures.
    readonly property bool sheetOpened: shareBookmarkSheet.opened || howItWorksSheet.opened

    text: d.shared ? qsTr("Shared") : qsTr("Share")

    icon.source: d.shared
        ? `image://skin/20x20/Solid/shared.svg?primary=light10&secondary=green`
        : `image://skin/20x20/Solid/share.svg?primary=light10`

    enabled: backend.isAvailable

    onTriggered: share()

    function share()
    {
        if (!backend.isAvailable)
            return

        if (d.needsHowItWork())
            howItWorksSheet.open()
        else
            shareBookmarkSheet.open()
    }

    readonly property NxObject d: NxObject
    {
        readonly property bool createsNewBookmark:
            action.objectsType !== Timeline.ObjectsLoader.ObjectsType.bookmarks

        readonly property bool shared: backend.isShared && !createsNewBookmark

        function needsHowItWork()
        {
            if (action.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics)
                return appContext.settings.showHowShareWorksNotification

            if (action.objectsType === Timeline.ObjectsLoader.ObjectsType.motion)
                return appContext.settings.showHowDetectedMotionShareWorksNotification

            return false
        }

        function storeDoNotShowAgain()
        {
            if (action.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics)
                appContext.settings.showHowShareWorksNotification = false

            if (action.objectsType === Timeline.ObjectsLoader.ObjectsType.motion)
                appContext.settings.showHowDetectedMotionShareWorksNotification = false
        }

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
                newBookmarkMode: d.createsNewBookmark
                preferredEdge: action.preferredSheetEdge

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

                doNotShowAgain: false
                preferredEdge: action.preferredSheetEdge

                onContinued:
                {
                    if (doNotShowAgain)
                        d.storeDoNotShowAgain()

                    shareBookmarkSheet.open()
                }
            }
        }
    }
}
