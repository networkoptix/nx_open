// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.core

ColoredImage

{
    id: control

    property alias resource: statusHelper.resource
    property alias color: control.primaryColor
    property bool useSmallIcon: false

    sourceSize: control.useSmallIcon
        ? statusHelper.smallIconSize
        : statusHelper.normalIconSize
    sourcePath: control.useSmallIcon
        ? statusHelper.qmlSmallIconName
        : statusHelper.qmlIconName
    primaryColor: ColorTheme.colors.red

    RecordingStatusHelper
    {
        id: statusHelper
    }
}
