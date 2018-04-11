.import Qt.labs.controls 1.0 as Controls

function focusCurrentScreen()
{
    if (stackView.currentItem)
        stackView.currentItem.forceActiveFocus()
}

function popCurrentScreen()
{
    if (stackView.depth > 1)
        stackView.pop().destroy()
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
        stackView.safeReplace(null, Qt.resolvedUrl("Screens/SessionsScreen.qml"))
    }
}

function openSessionsScreenWithWarning(systemName)
{
    var item = stackView.safeReplace(null, Qt.resolvedUrl("Screens/SessionsScreen.qml"))
    item.openConnectionWarningDialog(systemName)
}

function openNewSessionScreen()
{
    var item = stackView.safePush(Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"))
    item.focusHostField()
}

function openDiscoveredSession(systemId, localSystemId, systemName, address)
{
    var item = stackView.safePush(
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
            {
                "systemId": systemId,
                "localSystemId": localSystemId,
                "systemName": systemName,
                "address": address
            }
    )
    item.focusCredentialsField()
}

function openSavedSession(systemId, localSystemId, systemName, address, login, password)
{
    var item = stackView.safePush(
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
    item.focusCredentialsField()
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
        stackView.safeReplace(
            null,
            Qt.resolvedUrl("Screens/ResourcesScreen.qml"),
            {
                "title": systemName
            }
        )
    }
}

function openVideoScreen(resourceId, screenshotUrl, xHint, yHint)
{
    stackView.setScaleTransitionHint(xHint, yHint)
    var item = stackView.safePush(
        Qt.resolvedUrl("Screens/VideoScreen.qml"),
        {
            "resourceId": resourceId,
            "initialScreenshot": screenshotUrl
        })
    return item
}

function openSettingsScreen()
{
    stackView.safePush(Qt.resolvedUrl("Screens/SettingsScreen.qml"))
}

function openDeveloperSettingsScreen()
{
    stackView.safePush(Qt.resolvedUrl("Screens/DeveloperSettingsScreen.qml"))
}

function openCloudWelcomeScreen()
{
    stackView.safeReplace(null, Qt.resolvedUrl("Screens/Cloud/WelcomeScreen.qml"))
}

function openCloudScreen(user, password, connectOperationId)
{
    stackView.safePush(Qt.resolvedUrl("Screens/Cloud/CloudScreen.qml"),
        {
            "targetEmail": user,
            "targetPassword": password,
            "connectOperationId": connectOperationId
        })
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
        stackView.safeReplace(
            null,
            Qt.resolvedUrl("Screens/LiteClientControlScreen.qml"),
            {
                "clientId": clientId
            }
        )
    }
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
        stackView.safeReplace(null, Qt.resolvedUrl("Screens/LiteClientWelcomeScreen.qml"))
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
