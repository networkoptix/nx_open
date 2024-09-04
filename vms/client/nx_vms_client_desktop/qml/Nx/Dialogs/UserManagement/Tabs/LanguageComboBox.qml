// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls

import nx.vms.client.core

ComboBox
{
    id: control

    model: TranslationListModel {}
    withIconSection: true
    colorizeIcons: false
    currentIndex: 0
    textRole: "display"
    valueRole: "localeCode"

    view: ListView
    {
        clip: true
        itemHeightHint: 24
        implicitHeight: Math.min(itemHeightHint * 10, contentHeight)
        model: control.popup.visible ? control.delegateModel : null
        currentIndex: control.highlightedIndex
        boundsBehavior: ListView.StopAtBounds
    }
}
