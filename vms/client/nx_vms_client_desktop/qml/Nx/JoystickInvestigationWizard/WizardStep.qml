// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml 2.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0
import Nx.JoystickInvestigationWizard 1.0

import nx.vms.client.desktop 1.0

Item
{
    property bool nextEnabled: true

    signal beforeNext()
}
