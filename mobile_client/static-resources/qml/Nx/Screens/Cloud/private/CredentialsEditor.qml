import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Item
{
    property alias email: emailField.text
    property alias password: passwordField.text
    property alias learnMoreLinkVisible: learnMoreLink.visible

    signal loggedIn()

    implicitWidth: parent ? parent.width : 0
    implicitHeight: content.height

    property bool _invalidCredentials: false

    Column
    {
        id: content
        width: parent.width

        Column
        {
            width: parent.width
            topPadding: 24
            bottomPadding: 24

            TextField
            {
                id: emailField
                placeholderText: qsTr("E-mail")
                width: parent.width
                showError: _invalidCredentials
            }
            TextField
            {
                id: passwordField
                echoMode: TextInput.PasswordEchoOnEdit
                placeholderText: qsTr("Password")
                width: parent.width
                showError: _invalidCredentials
            }
        }

        Button
        {
            id: loginButton
            text: "Login"
            width: parent.width
            color: ColorTheme.brand_main
            onClicked: login()
        }

        Column
        {
            width: parent.width
            topPadding: 16
            bottomPadding: 16
            spacing: 8

            LinkButton
            {
                id: learnMoreLink
                text: qsTr("Learn mote about %1").arg(applicationInfo.cloudName())
                width: parent.width
            }
            LinkButton
            {
                text: qsTr("Create account")
                width: parent.width
            }
            LinkButton
            {
                text: qsTr("Forgot your password?")
                width: parent.width
            }
        }
    }

    Connections
    {
        target: cloudStatusWatcher

        onError:
        {
            if (errorCode == QnCloudStatusWatcher.InvalidCredentials)
            {
                _invalidCredentials = true
            }
            else
            {
                // TODO
            }
        }

        onStatusChanged:
        {
            if (status == QnCloudStatusWatcher.Online)
            {
                loggedIn()
                return
            }
        }
    }

    function login()
    {
        setCloudCredentials(email, password)
    }
}
