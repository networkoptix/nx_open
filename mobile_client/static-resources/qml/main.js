/*function openLoginDialog() {
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

*/

function openVideo(id) {
    pageStack.push({
        item: Qt.resolvedUrl("dialogs/QnVideoDialog.qml"),
        properties: {
            width: pageStack.width,
            height: pageStack.height,
            resourceId: id
        },
        destroyOnPop: true
    })
}

