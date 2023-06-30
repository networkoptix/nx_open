// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

PanelBase
{
    property var request: null

    description: request.description

    approveButtonVisible: true
    rejectButtonVisible: true

    headerItem: RequestHeader
    {
        name: request.name
        version: request.version
        authCode: request.pinCode
    }

    sourceItem: Text
    {
        id: address

        text: request.requestAddress

        font.pixelSize: 14
        color: ColorTheme.colors.dark14
    }
}
