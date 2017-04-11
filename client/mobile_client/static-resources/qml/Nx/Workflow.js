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
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName == "sessionsScreen")
    {
        if (stackView.depth > 1)
            stackView.pop(item)
    }
    else
    {
        item = stackView.replace(null, Qt.resolvedUrl("Screens/SessionsScreen.qml"))
        item.forceActiveFocus()
    }
}

function openSessionsScreenWithWarning(systemName)
{
    var item = stackView.replace(null, Qt.resolvedUrl("Screens/SessionsScreen.qml"))
    item.openConnectionWarningDialog(systemName)
}

function openNewSessionScreen()
{
    var item = stackView.push(Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"))
    item.focusHostField()
}

function openDiscoveredSession(systemId, localSystemId, systemName, address)
{
    var item = stackView.push(
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
            {
                "systemId": systemId,
                "localSystemId": localSystemId,
                "systemName": systemName,
                "address": address
            }
    )
    item.focusLoginField()
}

function openSavedSession(systemId, localSystemId, systemName, address, login, password)
{
    var item = stackView.push(
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
            {
                "systemId": systemId,
                "localSystemId": localSystemId,
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
    return item
}

function openSettingsScreen()
{
    var item = stackView.push(Qt.resolvedUrl("Screens/SettingsScreen.qml"))
    item.forceActiveFocus()
}

function openDeveloperSettingsScreen()
{
    var item = stackView.push(Qt.resolvedUrl("Screens/DeveloperSettingsScreen.qml"))
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

function openLiteClientControlScreen(clientId)
{
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName == "liteClientControlScreen")
    {
        item.clientId = clientId
        if (stackView.depth > 1)
            stackView.pop(item)
    }
    else
    {
        item = stackView.replace(
            null,
            Qt.resolvedUrl("Screens/LiteClientControlScreen.qml"),
            {
                "clientId": clientId
            }
        )
    }
    item.forceActiveFocus()
}

function openLiteClientWelcomeScreen()
{
    var item = stackView.get(0, Controls.StackView.ForceLoad)
    if (item && item.objectName == "liteClientWelcomeScreen")
    {
        if (stackView.depth > 1)
            stackView.pop(item)
    }
    else
    {
        item = stackView.replace(null, Qt.resolvedUrl("Screens/LiteClientWelcomeScreen.qml"))
        item.forceActiveFocus()
    }
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

function openStandardDialog(title, message, buttonsModel)
{
    return openDialog(
        "Dialogs/StandardDialog.qml",
        {
            "title": title,
            "message": message,
            "buttonsModel": buttonsModel ? buttonsModel : ["OK"]
        }
    )
}

function openOldClientDownloadSuggestion()
{
    return openDialog("Dialogs/DownloadOldClientDialog.qml")
}

function startTest(test)
{
    testLoader.source = "Test/" + test + ".qml"
    if (testLoader.item)
        testLoader.item.running = true
}

function stopTest()
{
    if (testLoader.item)
        testLoader.item.running = false
    testLoader.source = ""
}
