// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml.Models
import QtQuick

import Nx
import Nx.Controls

Instantiator
{
    id: instantiator

    required property Menu parentMenu
    required property int indexOffset

    required property Component separator

    required property AbstractItemModel sourceModel
    required property var rootIndex

    required property string textRole
    required property string dataRole
    required property string decorationPathRole
    required property string shortcutRole
    required property string enabledRole
    required property string visibleRole

    model: DelegateModel
    {
        id: delegateModel

        model: instantiator.sourceModel
        rootIndex: instantiator.rootIndex

        delegate: Loader
        {
            readonly property var inputData: ({
                "modelData": model,
                "modelIndex": NxGlobals.toPersistent(delegateModel.modelIndex(index))})

            property QtObject previousItem

            function updateSource()
            {
                sourceComponent = undefined

                if (!inputData.modelIndex.valid)
                    return

                if (NxGlobals.hasChildren(inputData.modelIndex))
                {
                    // ... - DynamicMenu - DynamicMenuItems - DynamicMenuItemInstantiator - ...
                    // circular dependency is relaxed here by loading the menu component from file.

                    setSource("../DynamicMenu.qml", {
                        "title": inputData.modelData[instantiator.textRole],
                        "delegate": parentMenu.delegate,
                        "separator": instantiator.separator,
                        "textRole": instantiator.textRole,
                        "dataRole": instantiator.dataRole,
                        "decorationPathRole": instantiator.decorationPathRole,
                        "shortcutRole": instantiator.shortcutRole,
                        "enabledRole": instantiator.enabledRole,
                        "visibleRole": instantiator.visibleRole,
                        "model": instantiator.sourceModel,
                        "rootIndex": inputData.modelIndex})
                }
                else
                {
                    sourceComponent = inputData.modelData[instantiator.textRole]
                        ? actionComponent
                        : instantiator.separator
                }
            }

            Component
            {
                id: actionComponent

                Action
                {
                    text: inputData.modelData[instantiator.textRole]
                    data: inputData.modelData[instantiator.dataRole]
                    icon.source: inputData.modelData[instantiator.decorationPathRole]
                    shortcut: inputData.modelData[instantiator.shortcutRole]
                    enabled: inputData.modelData[instantiator.enabledRole] ?? true
                    visible: inputData.modelData[instantiator.visibleRole] ?? true
                }
            }

            Component.onCompleted:
                updateSource()

            onInputDataChanged:
                updateSource()

            onItemChanged:
            {
                if (previousItem)
                {
                    instantiator.removeFromMenu(previousItem)
                    instantiator.addToMenu(index, item)
                }

                previousItem = item
            }
        }
    }

    function addToMenu(index, object)
    {
        if (!object)
            return

        if (object instanceof Menu)
            parentMenu.insertMenu(index + indexOffset, object)
        else if (object instanceof Action)
            parentMenu.insertAction(index + indexOffset, object)
        else if (object instanceof Item)
            parentMenu.insertItem(index + indexOffset, object)
    }

    function removeFromMenu(object)
    {
        if (!object)
            return

        if (object instanceof Menu)
            parentMenu.removeMenu(object)
        else if (object instanceof Action)
            parentMenu.removeAction(object)
        else if (object instanceof Item)
            parentMenu.removeItem(object)
    }

    onObjectAdded: (index, loader) => addToMenu(index, loader.item)
    onObjectRemoved: (index, loader) => removeFromMenu(loader.item)
}
