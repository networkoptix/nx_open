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

function openSessionsScreenWithWarning(message)
{
    var item = stackView.replace(null, Qt.resolvedUrl("Screens/SessionsScreen.qml"))
    item.forceActiveFocus()
    item.warningText = message
    item.warningVisible = true
}

function openNewSessionScreen()
{
    var item = stackView.push(Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"))
    item.focusHostField()
}

function openFailedSessionScreen(systemName, address, login, password, connectionStatus, info)
{
    var item = null
    if (stackView.get(0, Controls.StackView.ForceLoad).objectName == "sessionsScreen")
    {
        item = stackView.push(
                Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
                {
                    "systemName": systemName,
                    "address": address,
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
                    "systemName": systemName,
                    "address": address,
                    "login": login,
                    "password": password,
                }
        )
    }
    item.showWarning(connectionStatus, info)
    item.forceActiveFocus()
}

function openDiscoveredSession(systemName, address)
{
    var item = stackView.push(
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
            {
                "systemName": systemName,
                "address": address
            }
    )
    item.focusLoginField()
}

function openSavedSession(systemName, address, login, password)
{
    var item = stackView.push(
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
            {
                "systemName": systemName,
                "address": address,
                "login": login,
                "password": password,
                "saved": true
            }
    )
    item.forceActiveFocus()
}

function openResourcesScreen(systemName)
{
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName == "resourcesScreen")
    {
        if (stackView.depth > 1)
            stackView.pop(item)
    }
    else
    {
        item = stackView.replace(
                null,
                Qt.resolvedUrl("Screens/ResourcesScreen.qml"),
                {
                    "title": systemName
                }
        )
    }
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

function openCloudWelcomeScreen()
{
    var item = stackView.replace(null, Qt.resolvedUrl("Screens/Cloud/WelcomeScreen.qml"))
    item.forceActiveFocus()
}

function openCloudScreen()
{
    var item = stackView.push(Qt.resolvedUrl("Screens/Cloud/CloudScreen.qml"))
    item.forceActiveFocus()
}

function openDialog(path, properties)
{
    var component = Qt.createComponent(path)
    var dialog = component.createObject(mainWindow.contentItem, properties ? properties : {})
    dialog.open()
    return dialog
}

function openInformationDialog(title, message)
{
    openDialog(
        "Dialogs/InformationDialog.qml",
        {
            "title": title,
            "message": message
        }
    )
}

function openOldClientDownloadSuggestion()
{
    return openDialog("Dialogs/DownloadOldClientDialog.qml")
}
