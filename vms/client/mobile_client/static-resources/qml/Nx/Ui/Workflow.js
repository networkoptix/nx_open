// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

.import QtQuick.Controls 2.4 as Controls

function focusCurrentScreen()
{
    if (stackView.currentItem)
        stackView.currentItem.forceActiveFocus()
}

function popCurrentScreen()
{
    popScreens(1)
}

function popScreens(count)
{
    while (stackView.depth > 1 && count-- > 0)
        stackView.pop()

    stackView.currentItem.forceActiveFocus()
}

function hasScreenInStack(screenName)
{
    for (let i = 0; i !== stackView.children.length; ++i)
    {
        if (stackView.children[i].objectName === screenName)
            return true
    }
    return false
}

function openFeedScreen(feedState)
{
    stackView.replace(null, Qt.resolvedUrl("../Screens/FeedScreen.qml"), {"feedState": feedState})
}

function openSessionsScreen()
{
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName === "sessionsScreen")
    {
        if (stackView.depth > 1)
            stackView.pop(item)
    }
    else
    {
        stackView.replace(null, Qt.resolvedUrl("../Screens/SessionsScreen.qml"))
    }
}

function openDigestCloudConnectScreen(cloudSystemId, systemName)
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/Cloud/DigestLogin.qml"),
        {
            "cloudSystemId": cloudSystemId,
            "systemName": systemName,
            "login": cloudStatusWatcher.cloudLogin
        })
}

function openSessionsScreenWithWarning(systemName, errorText)
{
    var item = stackView.currentItem && stackView.currentItem.objectName == "sessionsScreen"
        ? stackView.currentItem
        : stackView.replace(null, Qt.resolvedUrl("../Screens/SessionsScreen.qml"))
    if (item)
        Qt.callLater(() => windowContext.ui.showConnectionErrorMessage(systemName, errorText))
}

function openSitePlaceholderScreen(systemName)
{
    stackView.pushScreen(
        Qt.resolvedUrl("../Screens/SitePlaceholderScreen.qml"),
        { "title": systemName })
}

function openConnectToServerScreen(host, user, password, operationId)
{
    var item = stackView.pushScreen(
        Qt.resolvedUrl("../Screens/CustomConnectionScreen.qml"),
        {
            "address": host,
            "login": user,
            "passowrd": password,
            "operationId": operationId
        })

    if (item)
        item.focusCredentialsField()
}

function openResourcesScreen(systemName, filterIds)
{
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName == "resourcesScreen")
    {
        item.filterIds = filterIds
        if (stackView.depth > 1)
            stackView.pop(item)
    }
    else
    {
        stackView.replace(
            null,
            Qt.resolvedUrl("../Screens/ResourcesScreen.qml"),
            {
                "title": systemName,
                "filterIds": filterIds
            }
        )
    }
}

function openVideoScreen(resource, screenshotUrl, xHint, yHint, timestamp, camerasModel)
{
    var targetTimestamp = timestamp > 0 ? timestamp : -1
    var properties =
        {
            "initialResource": resource,
            "initialScreenshot": screenshotUrl ?? "",
            "targetTimestamp": targetTimestamp
        }
    if (camerasModel)
        properties["camerasModel"] = camerasModel
    stackView.pushScreen(Qt.resolvedUrl("../Screens/VideoScreen.qml"), properties)
}

function openSettingsScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/SettingsScreen.qml"))
}

function openPushExpertModeScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/PushExpertModeScreen.qml"))
}

function openDeveloperSettingsScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/DeveloperSettingsScreen.qml"))
}

function openCloudSummaryScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/Cloud/Summary.qml"))
}

function openCloudLoginScreen(forced)
{
    stackView.pushScreen(
        Qt.resolvedUrl("../Screens/Cloud/Login.qml"),
        {
            "forced": forced ?? false
        })
}

function openSecuritySettingsScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/SecuritySettingsScreen.qml"))
}

function openCameraSettingsScreen(mediaPlayer, audioSupported, audioController)
{
    stackView.pushScreen(
        Qt.resolvedUrl("../Screens/CameraSettingsScreen.qml"),
        {
            "player": mediaPlayer,
            "audioSupported": audioSupported,
            "audioController": audioController
        })
}

function openEventSearchScreen(selectedResourceId, camerasModel, analyticsSearchMode)
{
    if (hasScreenInStack("eventSearchScreen"))
        popScreens(3) //< Pop video, event details, screen.

    stackView.pushScreen(Qt.resolvedUrl("../Screens/EventSearch/EventSearchScreen.qml"),
        {
            'camerasModel': camerasModel,
            'customResourceId': selectedResourceId,
            'analyticsSearchMode': !!analyticsSearchMode
        })
}

function openEventFiltersScreen(controller)
{
    stackView.pushScreen(
        Qt.resolvedUrl("../Screens/EventSearch/private/FiltersScreen.qml"),
        {
            "controller": controller,
        })
}

function openEventDetailsScreen(camerasModel, bookmarksModel, currentIndex, isAnalyticsDetails)
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/EventSearch/private/DetailsScreen.qml"),
        {
            "isAnalyticsDetails": isAnalyticsDetails,
            "camerasModel": camerasModel,
            "eventSearchModel": bookmarksModel,
            "currentEventIndex": currentIndex
        })
}

function openBetaFeaturesScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/BetaFeaturesScreen.qml"))
}

function openEventSearchMenuScreen()
{
    stackView.replace(null, Qt.resolvedUrl("../Screens/EventSearchMenuScreen.qml"))
}

function openMenuScreen()
{
    stackView.replace(null, Qt.resolvedUrl("../Screens/MenuScreen.qml"))
}

function openDialog(path, properties)
{
    var component = Qt.createComponent(path)
    var dialog = component.createObject(mainWindow.contentItem, properties ? properties : {})
    if (dialog)
        dialog.open()
    else
        console.log(component.errorString())
    return dialog
}

function openStandardDialog(title, message = "", buttonsModel = ["OK"], disableAutoClose = false)
{
    return openDialog(
        "../Dialogs/StandardDialog.qml",
        {
            "title": title,
            "message": message,
            "disableAutoClose": disableAutoClose,
            "buttonsModel": buttonsModel
        }
    )
}

function openInterfaceSettingsScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/InterfaceSettingsScreen.qml"))
}

function openPerformanceSettingsScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/PerformanceSettingsScreen.qml"))
}

function openAppInfoScreen()
{
    stackView.pushScreen(Qt.resolvedUrl("../Screens/AppInfoScreen.qml"))
}
