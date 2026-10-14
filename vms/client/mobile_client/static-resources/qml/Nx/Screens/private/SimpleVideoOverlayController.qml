// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.api

QtObject
{
    id: overlayController

    required property MediaResourceHelper mediaResourceHelper

    property int status: mediaResourceHelper.resource?.status ?? API.ResourceStatus.offline

    readonly property bool offline: status === API.ResourceStatus.offline
    readonly property bool unauthorized: status === API.ResourceStatus.unauthorized
    readonly property bool needsCloudAuthorization: mediaResourceHelper.needsCloudAuthorization
    readonly property bool is2FaDisabled: !appContext.cloudStatusWatcher.is2FaEnabledForUser
        && mediaResourceHelper.is2FaEnabled
    readonly property bool hasDefaultPassword: mediaResourceHelper.hasDefaultCameraPassword
    readonly property bool hasOldFirmware: mediaResourceHelper.hasOldCameraFirmware
    readonly property bool ioModule: !mediaResourceHelper.hasVideo && mediaResourceHelper.isIoModule

    readonly property string dummyState:
    {
        if (needsCloudAuthorization)
            return "needsCloudAuthorization"
        if (is2FaDisabled)
            return "is2FaDisabled"
        if (unauthorized)
            return "cameraUnauthorized"
        if (hasDefaultPassword)
            return "defaultPasswordAlert"
        if (hasOldFirmware)
            return "oldFirmwareAlert"
        if (offline)
            return "cameraOffline"
        if (ioModule)
            return "ioModuleWarning"
        return ""
    }
}
