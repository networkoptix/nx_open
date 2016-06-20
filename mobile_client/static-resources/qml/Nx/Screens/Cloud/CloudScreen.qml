import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private"

Page
{
    padding: 16
    topPadding: 0

    title: qsTr("Cloud Account")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    property bool loggedIn: cloudStatusWatcher.status != QnCloudStatusWatcher.LoggedOut

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
            sourceComponent: loggedIn ? summaryComponent
                                      : credentialsComponent
        }
    }

    Component
    {
        id: credentialsComponent

        CredentialsEditor
        {
            learnMoreLinkVisible: false
        }
    }

    Component
    {
        id: summaryComponent

        CloudSummary
        {
        }
    }
}
