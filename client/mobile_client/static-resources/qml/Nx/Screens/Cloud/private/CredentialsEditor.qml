import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Item
{
    id: control

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
        property bool reactivationRequested: false
        property string initialLogin
        property string lastNotActivatedAccount
    }

    onVisibleChanged: d.reactivationRequested = false

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
                showError: d.invalidCredentials || notActivatedWarningPanel.opened
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

            FieldWarning
            {
                id: notActivatedWarningPanel

                width: parent.availableWidth
                text: qsTr("Account not activated")
            }

            LinkButton
            {
                width: parent.width
                visible: notActivatedWarningPanel.opened && d.lastNotActivatedAccount.length > 0
                enabled: !d.connecting && !d.reactivationRequested
                opacity: enabled ? 1.0 : 0.3

                text: qsTr("Resend activation email")

                onClicked:
                {
                    d.reactivationRequested = true
                    cloudStatusWatcher.resendActivationEmail(d.lastNotActivatedAccount)
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
            enabled: !d.reactivationRequested
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

        onActivationEmailResent:
        {
            if (!control.visible)
                return

            var caption = success
                ? qsTr("Activation email sent")
                : qsTr("Can't send activation email")
            var text = success
                ? qsTr("Check your inbox and visit provided link to activate account")
                : qsTr("Check your internet connection or try again later")

            var dialog = Workflow.openStandardDialog("", text)
            d.reactivationRequested = false
        }

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
            else if (error = QnCloudStatusWatcher.AccountNotActivated)
            {
                d.lastNotActivatedAccount = emailField.text
                showNotActivatedAccoundWarning()
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
                d.reactivationRequested = false
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
        d.reactivationRequested = false
        setCloudCredentials(email, password)
    }

    function showNotActivatedAccoundWarning()
    {
        warningPanel.opened = false
        notActivatedWarningPanel.opened = true
    }

    function showWarning(text)
    {
        notActivatedWarningPanel.opened = false
        warningPanel.text = text
        warningPanel.opened = true
    }

    function hideWarning()
    {
        warningPanel.opened = false
        notActivatedWarningPanel.opened = false
    }

    function focusCredentialFields()
    {
        if (emailField.text.length)
            passwordField.forceActiveFocus()
        else
            emailField.forceActiveFocus()
    }

    Component.onCompleted:
    {
        d.initialLogin = cloudStatusWatcher.effectiveUserName
        emailField.text = d.initialLogin
        focusCredentialFields()
    }
}
