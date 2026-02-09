// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function openDefaultScreen()
{
    if (windowContext.sessionManager.hasSession)
        openResourcesScreen(windowContext.sessionManager.systemName)
    else
        openSessionsScreen()
}

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

function openSessionsScreen()
{
    var item = stackView.get(0, StackView.ForceLoad)
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
    var item = stackView.get(0, StackView.ForceLoad)
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

function openVideoScreen(resource, screenshotUrl, timestamp, camerasModel, selectedObjectsType)
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

    if (selectedObjectsType && appContext.settings.newTimelinePrototype)
        properties["selectedObjectsType"] = selectedObjectsType

    const screen = appContext.settings.newTimelinePrototype
        ? stackView.pushScreen(Qt.resolvedUrl("../Screens/VideoScreen.qml"), properties)
        : stackView.pushScreen(Qt.resolvedUrl("../Screens/DeprecatedVideoScreen.qml"), properties)
}

// Pushes or replaces Settings screen based on 'push' parameter. Pushing is required to be able
// to go back to the previous screen.
// At the moment `push` is used only by the FeedScreen to open the push notifications settings.
function openSettingsScreen(push, initialPage = "")
{
    const properties = {"initialPage": initialPage}

    if (push)
        stackView.pushScreen(Qt.resolvedUrl("../Screens/SettingsScreen.qml"), properties)
    else
        stackView.replace(null, Qt.resolvedUrl("../Screens/SettingsScreen.qml"), properties)
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

// Pushes or replaces Event Search screen based on 'push' parameter. Pushing is required to be able
// to go back to the previous screen.
// When the search screen is opened from the navigation bar it must replace all the items in stack,
// otherwise (e.g. when opened from video screen) it must be pushed onto the stack.
function openEventSearchScreen(push, selectedResourceId, camerasModel, analyticsSearchMode)
{
    if (hasScreenInStack("eventSearchScreen"))
        popScreens(3) //< Pop video, event details, screen.

    let properties = {}
    if (selectedResourceId)
        properties['customResourceId'] = selectedResourceId

    if (camerasModel)
        properties['camerasModel'] = camerasModel

    if (analyticsSearchMode)
        properties['analyticsSearchMode'] = analyticsSearchMode

    if (push)
        stackView.push(Qt.resolvedUrl("../Screens/EventSearch/EventSearchScreen.qml"), properties)
    else
        stackView.replace(null, Qt.resolvedUrl("../Screens/EventSearch/EventSearchScreen.qml"), properties)
}

function openFeedScreen(push, feedState)
{
    if (push)
        stackView.push(Qt.resolvedUrl("../Screens/FeedScreen.qml"), {"feedState": feedState})
    else
        stackView.replace(null, Qt.resolvedUrl("../Screens/FeedScreen.qml"), {"feedState": feedState})
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
