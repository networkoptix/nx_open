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

        property bool invalidUser: false
        property bool invalidPassword: false
        property bool notActivated: false
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
                showError: d.invalidUser || d.notActivated
                activeFocusOnTab: true
                inputMethodHints:
                    Qt.ImhSensitiveData
                        | Qt.ImhEmailCharactersOnly
                        | Qt.ImhPreferLatin
                        | Qt.ImhNoAutoUppercase

                onAccepted: nextItemInFocusChain(true).forceActiveFocus()
                onActiveFocusChanged:
                {
                    if (activeFocus && (d.invalidUser || d.notActivated))
                        hideWarnings()
                }
            }

            FieldWarning
            {
                id: accountWarningPanel

                width: parent.availableWidth
            }

            LinkButton
            {
                width: parent.width
                visible: d.notActivated && d.lastNotActivatedAccount.length > 0
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
                showError: d.invalidPassword
                activeFocusOnTab: true
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhPreferLatin

                onAccepted: login()
                onActiveFocusChanged:
                {
                    if (activeFocus && d.invalidPassword)
                        hideWarnings()
                }
            }

            FieldWarning
            {
                id: bottomWarningPanel
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
                : qsTr("Cannot send activation email")
            var text = success
                ? qsTr("Check your inbox and visit provided link to activate account")
                : qsTr("Check your internet connection or try again later")

            var dialog = Workflow.openStandardDialog("", text)
            d.reactivationRequested = false
        }

        onErrorChanged: handleConnectStatusChanged()
        onStatusChanged: handleConnectStatusChanged()
    }

    function handleConnectStatusChanged()
    {
        var status = cloudStatusWatcher.status
        if (status == QnCloudStatusWatcher.Online)
        {
            loggedIn()
            d.connecting = false
            d.reactivationRequested = false
            return
        }

        var error = cloudStatusWatcher.error
        if (error == QnCloudStatusWatcher.NoError)
            return

        d.connecting = false
        if (error == QnCloudStatusWatcher.InvalidUser)
        {
            d.invalidUser = true
            showAccountWarning(qsTr("Account not found"))
            passwordField.text = ""
        }
        else if (error == QnCloudStatusWatcher.InvalidPassword)
        {
            d.invalidPassword = true
            showBottomWarning(qsTr("Wrong password"))
            passwordField.text = ""
        }
        else if (error == QnCloudStatusWatcher.AccountNotActivated)
        {
            d.notActivated = true
            d.lastNotActivatedAccount = emailField.text
            showAccountWarning(qsTr("Account not activated"))
            passwordField.text = ""
        }
        else
        {
            showBottomWarning(qsTr("Cannot connect to %1").arg(applicationInfo.cloudName()))
        }
    }

    function login()
    {
        hideWarnings()
        loginButton.forceActiveFocus()

        if (!emailField.text.trim())
        {
            d.invalidUser = true
            showAccountWarning(qsTr("Email cannot be empty"))
            return
        }

        if (!passwordField.text.trim())
        {
            d.invalidPassword = true
            showBottomWarning(qsTr("Password cannot be empty"))
            return
        }

        d.connecting = true
        d.reactivationRequested = false
        if (!setCloudCredentials(email, password))
            handleConnectStatusChanged()
    }

    function showAccountWarning(text)
    {
        bottomWarningPanel.opened = false

        accountWarningPanel.text = text
        accountWarningPanel.opened = true
    }

    function showBottomWarning(text)
    {
        accountWarningPanel.opened = false

        bottomWarningPanel.text = text
        bottomWarningPanel.opened = true
    }

    function hideWarnings()
    {
        bottomWarningPanel.opened = false
        accountWarningPanel.opened = false
        d.invalidUser = false
        d.invalidPassword = false
        d.notActivated = false
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
