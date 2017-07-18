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
            leftPadding: 8
            rightPadding: 8
            topPadding: 24
            bottomPadding: 24

            property real availableWidth: width - leftPadding - rightPadding

            TextField
            {
                id: emailField
                placeholderText: qsTr("Email")
                width: parent.availableWidth
                showError: d.invalidCredentials
                activeFocusOnTab: true
                inputMethodHints:
                    Qt.ImhSensitiveData
                        | Qt.ImhEmailCharactersOnly
                        | Qt.ImhPreferLatin
                        | Qt.ImhNoAutoUppercase

                onAccepted: nextItemInFocusChain(true).forceActiveFocus()
                onActiveFocusChanged:
                {
                    if (activeFocus && d.invalidCredentials)
                    {
                        d.invalidCredentials = false
                        hideWarning()
                    }
                }
            }
            TextField
            {
                id: passwordField
                echoMode: TextInput.Password
                passwordMaskDelay: 1500
                placeholderText: qsTr("Password")
                width: parent.availableWidth
                showError: d.invalidCredentials
                activeFocusOnTab: true
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhPreferLatin

                onAccepted: login()
                onActiveFocusChanged:
                {
                    if (activeFocus && d.invalidCredentials)
                    {
                        d.invalidCredentials = false
                        hideWarning()
                    }
                }
            }
            FieldWarning
            {
                id: warningPanel
                width: parent.availableWidth
            }
        }

        LoginButton
        {
            id: loginButton
            text: qsTr("Log in")
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
                text: qsTr("Learn more about %1").arg(applicationInfo.cloudName())
                width: parent.width
                onClicked: Qt.openUrlExternally(cloudUrlHelper.aboutUrl())
            }
            LinkButton
            {
                text: qsTr("Create account")
                width: parent.width
                onClicked: Qt.openUrlExternally(cloudUrlHelper.createAccountUrl())
            }
            LinkButton
            {
                text: qsTr("Forgot your password?")
                width: parent.width
                onClicked: Qt.openUrlExternally(cloudUrlHelper.restorePasswordUrl())
            }
        }
    }

    Connections
    {
        target: cloudStatusWatcher

        onErrorChanged:
        {
            if (error == QnCloudStatusWatcher.NoError)
                return

            d.connecting = false
            if (error == QnCloudStatusWatcher.InvalidCredentials)
            {
                d.invalidCredentials = true
                showWarning(qsTr("Incorrect email or password"))
            }
            else
            {
                showWarning(qsTr("Cannot connect to %1").arg(applicationInfo.cloudName()))
            }
            setCloudCredentials(d.initialLogin, "")
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
            showWarning(qsTr("Email and password cannot be empty"))
            return
        }

        hideWarning()
        d.invalidCredentials = false
        d.connecting = true
        setCloudCredentials(email, password)
    }

    function showWarning(text)
    {
        warningPanel.text = text
        warningPanel.opened = true
    }

    function hideWarning()
    {
        warningPanel.opened = false
    }

    Component.onCompleted:
    {
        d.initialLogin = cloudStatusWatcher.effectiveUserName
        emailField.text = d.initialLogin
        emailField.forceActiveFocus()
    }
}
