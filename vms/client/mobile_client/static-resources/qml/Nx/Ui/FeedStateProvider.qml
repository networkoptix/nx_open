// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.mobile

NxObject
{
    id: feedState

    readonly property alias notifications: notifications.notifications
    readonly property alias unviewedCount: notifications.unviewedCount
    readonly property alias notificationsEnabled: d.notificationsEnabled
    readonly property bool hasOsPermission: appContext.pushManager.hasOsPermission
    readonly property bool active: hasOsPermission && notificationsEnabled
    property alias cloudSystemIds: notifications.cloudSystemIds
    property alias user: notifications.user
    property int updateIntervalMs: 4000

    property string iconColor: "light4"
    readonly property string buttonIconSource: "image://skin/24x24/Outline/%1.svg?primary=%2"
       .arg(active ? "notification" : "notification_off")
       .arg(iconColor)

    readonly property string buttonIconIndicatorText: active && unviewedCount > 0
        ? (unviewedCount > 9 ? "9+" : unviewedCount)
        : ""

    function update() { notifications.update() }
    function setViewed(id, value) { notifications.setViewed(id, value) }

    NxObject
    {
        id: d

        readonly property var allSelectedSystems: new Set(appContext.pushManager.selectedSystems)

        readonly property var selectedSystems: appContext.pushManager.expertMode
            ? feedState.cloudSystemIds.filter(item => allSelectedSystems.has(item))
            : feedState.cloudSystemIds

        readonly property bool notificationsEnabled:
            appContext.pushManager.enabledCheckState !== Qt.Unchecked
                && selectedSystems.length !== 0

        PushNotificationProvider
        {
            id: notifications

            readonly property bool activeSession: windowContext.sessionManager.hasActiveSession
                || windowContext.sessionManager.hasReconnectingSession

            user: appContext.cloudStatusWatcher.cloudLogin
            cloudSystemIds: activeSession ? [windowContext.mainSystemContext.cloudSystemId] : []
        }

        Timer
        {
            running: true
            interval: feedState.updateIntervalMs
            repeat: true
            triggeredOnStart: true
            onTriggered: feedState.update()
        }
    }
}
