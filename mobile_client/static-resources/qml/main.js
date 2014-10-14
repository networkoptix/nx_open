function openLoginDialog() {
    stackView.push({
        item: Qt.resolvedUrl("dialogs/QnLoginDialog.qml"),
        properties: {
            width: stackView.width,
            height: stackView.height
        }
    })
}

function openResources() {
    stackView.push({
        item: Qt.resolvedUrl("dialogs/QnResourcesDialog.qml"),
        properties: {
            width: stackView.width,
            height: stackView.height
        },
        replace: true
    })
}
