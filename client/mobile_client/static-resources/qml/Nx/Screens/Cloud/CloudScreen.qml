import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private"

Page
{
    id: cloudScreen

    padding: 8
    topPadding: 0

    title: qsTr("Cloud Account")
    onLeftButtonClicked: Workflow.popCurrentScreen()

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
        content.sourceComponent =
            (cloudStatusWatcher.status == QnCloudStatusWatcher.Online
                || cloudStatusWatcher.status == QnCloudStatusWatcher.Offline)
                ? summaryComponent
                : credentialsComponent;
    }
}
