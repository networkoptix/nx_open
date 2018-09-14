.import QtQuick.Controls 2.4 as Controls

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
    if (item)
        item.openConnectionWarningDialog(systemName)
}

function openNewSessionScreen()
{
    var item = stackView.safePush(Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"))
    if (item)
        item.focusHostField()
}

function openConnectToServerScreen(host, user, password, operationId)
{
    var item = stackView.safePush(
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
            {
                "address": address,
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
            Qt.resolvedUrl("Screens/CustomConnectionScreen.qml"),
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
            Qt.resolvedUrl("Screens/ResourcesScreen.qml"),
            {
                "title": systemName,
                "filterIds": filterIds
            }
        )
    }
}

function openVideoScreen(resourceId, screenshotUrl, xHint, yHint, timestamp)
{
    var targetTimestamp = timestamp > 0 ? timestamp : -1
    var properties =
        {
            "resourceId": resourceId,
            "initialScreenshot": screenshotUrl,
            "targetTimestamp": targetTimestamp
        }
    stackView.setScaleTransitionHint(xHint, yHint)
    return stackView.safePush(Qt.resolvedUrl("Screens/VideoScreen.qml"), properties)
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
