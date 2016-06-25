import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Item
{
    property alias email: emailField.text
    property alias password: passwordField.text
    property alias learnMoreLinkVisible: learnMoreLink.visible
    readonly property bool connecting: d.connecting
    property WarningPanel warningPanel: null

    signal loggedIn()

    implicitWidth: parent ? parent.width : 0
    implicitHeight: content.height

    QtObject
    {
        id: d

        property bool invalidCredentials: false
        property bool connecting: false
        property string initialLogin
    }

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
                showError: d.invalidCredentials
                activeFocusOnTab: true
            }
            TextField
            {
                id: passwordField
                echoMode: TextInput.Password
                passwordMaskDelay: 1500
                placeholderText: qsTr("Password")
                width: parent.width
                showError: d.invalidCredentials
                activeFocusOnTab: true
                onAccepted: login()
            }
        }

        LoginButton
        {
            id: loginButton
            text: "Login"
            width: parent.width
            showProgress: d.connecting
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
            setCloudCredentials(d.initialLogin, "")
            d.connecting = false
            if (errorCode == QnCloudStatusWatcher.InvalidCredentials)
            {
                d.invalidCredentials = true
                showWarning(qsTr("Invalid e-mail or password"))
            }
            else
            {
                showWarning(qsTr("Cannot connect to %1").arg(applicationInfo.cloudName()))
            }
        }

        onStatusChanged:
        {
            if (status == QnCloudStatusWatcher.Online)
            {
                loggedIn()
                d.connecting = false
                return
            }
        }
    }

    function login()
    {
        loginButton.forceActiveFocus()

        if (!emailField.text || !passwordField.text)
        {
            d.invalidCredentials = true
            showWarning(qsTr("Invalid e-mail or password"))
            return
        }

        hideWarning()
        d.invalidCredentials = false
        d.connecting = true
        setCloudCredentials(email, password)
    }

    function showWarning(text)
    {
        if (!warningPanel)
            return

        warningPanel.text = text
        warningPanel.opened = true
    }

    function hideWarning()
    {
        if (!warningPanel)
            return

        warningPanel.opened = false
    }

    Component.onCompleted:
    {
        d.initialLogin = cloudStatusWatcher.cloudLogin
        emailField.text = d.initialLogin
    }
}
