// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

MenuSection
{
    id: control

    property var engineId
    property var sectionId: null
    property var requestId
    property var settingsModel
    property string type: ""

    text: (settingsModel && settingsModel.caption) || ""
    itemId: navigationMenu.getId(engineId, sectionId, requestId)
    collapsible:
        !!settingsModel
        && !!settingsModel.sections
        && (settingsModel.sections.length > 0)
        && active

    iconSource:
    {
        if (type === "api" && !requestId)
            return "image://skin/20x20/Solid/api.svg"
        if (type === "api" && !!engineId && !!requestId)
            return "image://skin/20x20/Solid/api_permission_changed.svg?secondary=yellow_l"
        if (type === "api" && !engineId && !!requestId)
            return "image://skin/20x20/Solid/api_not_approved.svg?secondary=red_l"

        return "image://skin/20x20/Solid/sdk.svg"
    }

    onClicked:
    {
        navigationMenu.setCurrentItem(engineId, sectionId, requestId)
    }

    content: Column
    {
        Repeater
        {
            model: settingsModel && settingsModel.sections

            Loader
            {
                // Using Loader because QML does not allow recursive components.

                width: parent.width

                Component.onCompleted:
                {
                    const model = settingsModel.sections[index]
                    setSource("EngineMenuItem.qml",
                        {
                            "active": Qt.binding(() => active),
                            "engineId": engineId,
                            "navigationMenu": navigationMenu,
                            "selected": Qt.binding(() => selected),
                            "settingsModel": model,
                            "sectionId": model.name,
                            "level": level + 1,
                        })
                }
            }
        }
    }
}
