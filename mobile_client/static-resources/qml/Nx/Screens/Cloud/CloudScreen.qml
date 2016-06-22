import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private"

Page
{
    id: cloudScreen

    padding: 16
    topPadding: 0

    title: qsTr("Cloud Account")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    Object
    {
        id: d

        property bool dynamicUpdate: true
        property bool loggedIn: false

        Binding
        {
            target: d
            property: "loggedIn"
            value: d.isLoggedIn()
            when: d.dynamicUpdate
        }

        function isLoggedIn()
        {
            return cloudStatusWatcher.status == QnCloudStatusWatcher.Online ||
                   cloudStatusWatcher.status == QnCloudStatusWatcher.Offline
        }
    }

    property bool loggedIn: false

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
            sourceComponent: d.loggedIn ? summaryComponent
                                        : credentialsComponent
        }
    }

    Component
    {
        id: credentialsComponent

        CredentialsEditor
        {
            id: credentialsEditor

            learnMoreLinkVisible: false
            warningPanel: cloudScreen.warningPanel

            onLoggedIn:
            {
                d.loggedIn = true
            }

            Binding
            {
                target: d
                property: "dynamicUpdate"
                value: !credentialsEditor.connecting
            }
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
