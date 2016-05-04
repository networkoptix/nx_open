function popCurrentScreen()
{
    if (stackView.depth > 1)
        stackView.pop()
}

function openSessionsScreen()
{
    stackView.replace(null, Qt.resolvedUrl("/qml/Nx/Screens/SessionsScreen.qml"))
}

function openNewSessionScreen()
{
    var item = stackView.push(Qt.resolvedUrl("/qml/Nx/Screens/CustomConnectionScreen.qml"))
    item.focusHostField()
}

function openFailedSessionScreen(sessionId, systemName, host, port, login, password, connectionStatus, info)
{
    var item = stackView.push(
            Qt.resolvedUrl("/qml/Nx/Screens/CustomConnectionScreen.qml"),
            {
                "sessionId": sessionId,
                "title": systemName,
                "host": host,
                "port": port,
                "login": login,
                "password": password,
            }
    )
    item.showWarning(connectionStatus, info)
}

function openDiscoveredSession(systemName, host, port)
{
    var item = stackView.push(
            Qt.resolvedUrl("/qml/Nx/Screens/CustomConnectionScreen.qml"),
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
            Qt.resolvedUrl("/qml/Nx/Screens/SavedConnectionScreen.qml"),
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
    stackView.replace(
            null,
            Qt.resolvedUrl("/qml/Nx/Screens/ResourcesScreen.qml"),
            {
                "title": systemName
            }
    )
}

function openVideoScreen(resourceId, screenshotUrl, xHint, yHint)
{
    stackView.setScaleTransitionHint(xHint, yHint)
    stackView.push(
            Qt.resolvedUrl("/qml/Nx/Screens/VideoScreen.qml"),
            {
                "resourcdId": resourceId,
                "initialScreenshot": screenshotUrl
            }
    )
}

function openSettingsScreen(systemName)
{
    stackView.push(Qt.resolvedUrl("/qml/Nx/Screens/SettingsScreen.qml"))
}
