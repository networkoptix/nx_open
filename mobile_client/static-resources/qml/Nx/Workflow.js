.import Qt.labs.controls 1.0 as Controls

function focusCurrentScreen()
{
    if (stackView.currentItem)
        stackView.currentItem.forceActiveFocus()
}

function popCurrentScreen()
{
    if (stackView.depth > 1)
        stackView.pop()
    stackView.currentItem.forceActiveFocus()
}

function openSessionsScreen()
{
    var item = stackView.replace(null, Qt.resolvedUrl("Screens/SessionsScreen.qml"))
    item.forceActiveFocus()
}

function openNewSessionScreen()
{
    var item = stackView.push(Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"))
    item.focusHostField()
}

function openFailedSessionScreen(sessionId, systemName, host, port, login, password, connectionStatus, info)
{
    var item = null
    if (stackView.get(0, Controls.StackView.ForceLoad).objectName == "sessionsScreen")
    {
        item = stackView.push(
                Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
                {
                    "sessionId": sessionId,
                    "title": systemName,
                    "host": host,
                    "port": port,
                    "login": login,
                    "password": password,
                }
        )
    }
    else
    {
        item = stackView.replace(
                null,
                Qt.resolvedUrl("Screens/SessionsScreen.qml"),
                {},
                Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
                {
                    "sessionId": sessionId,
                    "title": systemName,
                    "host": host,
                    "port": port,
                    "login": login,
                    "password": password,
                }
        )
    }
    item.showWarning(connectionStatus, info)
    item.forceActiveFocus()
}

function openDiscoveredSession(systemName, host, port)
{
    var item = stackView.push(
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
            {
                "title": systemName,
                "host": host,
                "port": port
            }
    )
    item.focusLoginField()
}

function openSavedSession(sessionId, systemName, host, port, login, password)
{
    var item = stackView.push(
            Qt.resolvedUrl("Screens/SavedConnectionScreen.qml"),
            {
                "sessionId": sessionId,
                "title": systemName,
                "host": host,
                "port": port,
                "login": login,
                "password": password
            }
    )
    item.forceActiveFocus()
}

function openResourcesScreen(systemName)
{
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName == "resourcesScreen")
        return

    item = stackView.replace(
            null,
            Qt.resolvedUrl("Screens/ResourcesScreen.qml"),
            {
                "title": systemName
            }
    )
    item.forceActiveFocus()
}

function openVideoScreen(resourceId, screenshotUrl, xHint, yHint)
{
    stackView.setScaleTransitionHint(xHint, yHint)
    var item = stackView.push(
            Qt.resolvedUrl("Screens/VideoScreen.qml"),
            {
                "resourceId": resourceId,
                "initialScreenshot": screenshotUrl
            }
    )
    item.forceActiveFocus()
}

function openSettingsScreen(systemName)
{
    var item = stackView.push(Qt.resolvedUrl("Screens/SettingsScreen.qml"))
    item.forceActiveFocus()
}

function openOldClientDownloadSuggestion()
{
    var component = Qt.createComponent("Dialogs/DownloadOldClientDialog.qml")
    var dialog = component.createObject(stackView)
    dialog.open()
    dialog.forceActiveFocus()
}
