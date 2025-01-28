// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls

import nx.vms.client.core

ComboBox
{
    id: control

    property string value: Branding.defaultLocale()

    model: TranslationListModel {}
    withIconSection: true
    colorizeIcons: false
    textRole: "display"
    valueRole: "localeCode"

    onCurrentValueChanged: control.value = currentValue ?? Branding.defaultLocale()

    onValueChanged:
    {
        const localeIndex = model.localeIndex(value)
        if (currentIndex !== localeIndex)
            currentIndex = localeIndex
    }

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
