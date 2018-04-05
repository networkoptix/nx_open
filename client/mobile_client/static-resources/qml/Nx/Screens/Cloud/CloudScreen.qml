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

    title: qsTr("Cloud Account")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    property string targetEmail
    property string targetPassword
    property bool forceLoginScreen: false

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
            onLoggedIn: Workflow.popCurrentScreen()
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

    Component.onCompleted:
    {
        var showSummary = !cloudScreen.forceLoginScreen &&
            (cloudStatusWatcher.status == QnCloudStatusWatcher.Online
            || cloudStatusWatcher.status == QnCloudStatusWatcher.Offline)

        cloudScreen.forceLoginScreen = false;
        content.sourceComponent = showSummary ? summaryComponent : credentialsComponent;
        if (showSummary)
            return

        content.item.email = cloudScreen.targetEmail
        content.item.password = cloudScreen.targetPassword
        content.item.focusCredentialFields()
    }
}
