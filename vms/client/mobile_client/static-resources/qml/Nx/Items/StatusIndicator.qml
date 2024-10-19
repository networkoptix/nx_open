// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Nx.Core

import nx.vms.api

Rectangle
{
    id: statusIndicator

    property int status

    property bool _offline: status == API.ResourceStatus.offline ||
                            status == API.ResourceStatus.undefined
    property bool _unauthorized: status == API.ResourceStatus.unauthorized
    property bool _recording: status == API.ResourceStatus.recording

    implicitWidth: 10
    implicitHeight: implicitWidth
    radius: height / 2

    color: !_offline && !_unauthorized && _recording ? ColorTheme.colors.red_core : "transparent"
    border.color: (_offline || _unauthorized) ? ColorTheme.colors.dark11
                                              : _recording ? ColorTheme.colors.red_core
                                                           : ColorTheme.windowText
}
