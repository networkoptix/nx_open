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
        stackView.pop().destroy()

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
        stackView.safeReplace(null, Qt.resolvedUrl("../Screens/SessionsScreen.qml"))
    }
}

function openDigestCloudConnectScreen(cloudSystemId, systemName)
{
    stackView.safePush(Qt.resolvedUrl("../Screens/Cloud/DigestLogin.qml"),
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
        : stackView.safeReplace(null, Qt.resolvedUrl("../Screens/SessionsScreen.qml"))
    if (item)
        showConnectionErrorMessage(systemName, errorText)
}

function openNewSessionScreen()
{
    var item = stackView.safePush(Qt.resolvedUrl("../Screens/CustomConnectionScreen.qml"))
    if (item)
        item.focusHostField()
}

function openConnectToServerScreen(host, user, password, operationId)
{
    var item = stackView.safePush(
            Qt.resolvedUrl("../Screens/CustomConnectionScreen.qml"),
            {
                "address": host,
                "login": user,
                "passowrd": password,
                "operationId": operationId
            }
    )
    if (item)
        item.focusCredentialsField()
}

function openDiscoveredSession(systemId, localSystemId, systemName, address)
{
    var item = stackView.safePush(
            Qt.resolvedUrl("../Screens/CustomConnectionScreen.qml"),
            {
                "systemId": systemId,
                "localSystemId": localSystemId,
                "systemName": systemName,
                "address": address
            }
    )
    if (item)
        item.focusCredentialsField()
}

function openSavedSession(
    systemId,
    localSystemId,
    systemName,
    address,
    login,
    passwordErrorText)
{

    var item = stackView.safePush(
        Qt.resolvedUrl("../Screens/CustomConnectionScreen.qml"),
        {
            "systemId": systemId,
            "localSystemId": localSystemId,
            "systemName": systemName,
            "address": address,
            "login": login,
            "saved": true,
            "passwordErrorText": passwordErrorText
        }
    )

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
        stackView.safeReplace(
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
    stackView.setScaleTransitionHint(xHint, yHint)
    const screen = stackView.safePush(Qt.resolvedUrl("../Screens/VideoScreen.qml"), properties)
    if (screen && camerasModel)
        screen.camerasModel = camerasModel
}

function openSettingsScreen()
{
    stackView.safePush(Qt.resolvedUrl("../Screens/SettingsScreen.qml"))
}

function openPushExpertModeScreen()
{
    stackView.safePush(Qt.resolvedUrl("../Screens/PushExpertModeScreen.qml"))
}

function openDeveloperSettingsScreen()
{
    stackView.safePush(Qt.resolvedUrl("../Screens/DeveloperSettingsScreen.qml"))
}

function openCloudSummaryScreen()
{
    stackView.safePush(Qt.resolvedUrl("../Screens/Cloud/Summary.qml"))
}

function openCloudLoginScreen()
{
    stackView.safePush(Qt.resolvedUrl("../Screens/Cloud/Login.qml"))
}

function openSecuritySettingsScreen()
{
    stackView.safePush(Qt.resolvedUrl("../Screens/SecuritySettingsScreen.qml"))
}

function openCameraSettingsScreen(mediaPlayer, audioSupported, audioController)
{
    stackView.safePush(
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
        popScreens(2) //< Pop video and event details screen.

    stackView.safePush(Qt.resolvedUrl("../Screens/EventSearch/SearchScreen.qml"),
        {
            'camerasModel': camerasModel,
            'customResourceId': selectedResourceId,
            'analyticsSearchMode': !!analyticsSearchMode
        })
}

function openEventFiltersScreen(controller)
{
    stackView.safePush(
        Qt.resolvedUrl("../Screens/EventSearch/private/FiltersScreen.qml"),
        {
            "controller": controller,
        })
}

function openEventDetailsScreen(camerasModel, bookmarksModel, currentIndex, isAnalyticsDetails)
{
    stackView.safePush(Qt.resolvedUrl("../Screens/EventSearch/private/DetailsScreen.qml"),
        {
            "isAnalyticsDetails": isAnalyticsDetails,
            "camerasModel": camerasModel,
            "eventSearchModel": bookmarksModel,
            "currentEventIndex": currentIndex
        })
}

function openBetaFeaturesScreen()
{
    stackView.safePush(Qt.resolvedUrl("../Screens/BetaFeaturesScreen.qml"))
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

function openStandardDialog(title, message, buttonsModel, disableAutoClose = false)
{
    return openDialog(
        "../Dialogs/StandardDialog.qml",
        {
            "title": title,
            "message": message,
            "disableAutoClose": disableAutoClose,
            "buttonsModel": buttonsModel ? buttonsModel : ["OK"]
        }
    )
}
