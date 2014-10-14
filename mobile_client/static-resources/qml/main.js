function openLoginDialog() {
    stackView.push({
        item: Qt.resolvedUrl("dialogs/QnLoginDialog.qml"),
        properties: {
            anchors: {
                fill: stackView
            }
        }
    })
}

function openResources() {
    stackView.push({
        item: Qt.resolvedUrl("dialogs/QnResourcesDialog.qml"),
        properties: {
            anchors: {
                fill: stackView
            }
        },
        replace: true
    })
}
