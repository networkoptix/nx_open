.import Qt.labs.controls 1.0 as Controls

function popCurrentScreen()
{
    if (stackView.depth > 1)
        stackView.pop()
}

function openSessionsScreen()
{
    stackView.replace(null, Qt.resolvedUrl("Screens/SessionsScreen.qml"))
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
    stackView.push(
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
}

function openResourcesScreen(systemName)
{
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName == "resourcesScreen")
        return

    stackView.replace(
            null,
            Qt.resolvedUrl("Screens/ResourcesScreen.qml"),
            {
                "title": systemName
            }
    )
}

function openVideoScreen(resourceId, screenshotUrl, xHint, yHint)
{
    stackView.setScaleTransitionHint(xHint, yHint)
    stackView.push(
            Qt.resolvedUrl("Screens/VideoScreen.qml"),
            {
                "resourceId": resourceId,
                "initialScreenshot": screenshotUrl
            }
    )
}

function openSettingsScreen(systemName)
{
    stackView.push(Qt.resolvedUrl("Screens/SettingsScreen.qml"))
}
