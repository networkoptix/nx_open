// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Dialogs
import Nx.JoystickInvestigationWizard

import nx.vms.client.desktop

Item
{
    property bool nextEnabled: true

    signal beforeNext()
}
