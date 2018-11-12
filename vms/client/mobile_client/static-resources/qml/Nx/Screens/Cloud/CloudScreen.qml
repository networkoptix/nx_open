import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private"

Page
{
    id: cloudScreen
    objectName: "cloudScreen"

    padding: 8
    topPadding: 0

    title: qsTr("%1 Account", "%1 is the short cloud name (like 'Cloud')")
        .arg(applicationInfo.shortCloudName())
    onLeftButtonClicked: completeConnectOperation(false)

    property string targetEmail
    property string targetPassword
    property string connectOperationId

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: content.width
        contentHeight: content.height

        Loader
        {
            id: content
            width: flickable.width
        }
    }

    Component
    {
        id: credentialsComponent

        CredentialsEditor
        {
            id: credentialsEditor
            learnMoreLinkVisible: false
            onLoggedIn: completeConnectOperation(true)
        }
    }

    Component
    {
        id: summaryComponent

        CloudSummary
        {
            onLoggedOut: Workflow.popCurrentScreen()
        }
    }

    function completeConnectOperation(success)
    {
        Workflow.popCurrentScreen()
        if (connectOperationId.length)
            operationManager.finishOperation(connectOperationId, success)

        connectOperationId = ""
    }

    Component.onCompleted:
    {
        var showSummary = !cloudScreen.connectOperationId.length &&
            (cloudStatusWatcher.status == QnCloudStatusWatcher.Online
            || cloudStatusWatcher.status == QnCloudStatusWatcher.Offline)

        cloudScreen.connectOperationId = ""
        content.sourceComponent = showSummary ? summaryComponent : credentialsComponent;
        if (showSummary)
            return

        content.item.email = cloudScreen.targetEmail
        content.item.password = cloudScreen.targetPassword
        content.item.focusCredentialFields()
    }
}
