import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"
import "../items"
import "../icons"
import "../main.js" as Main
import "QnLoginPage.js" as LoginFunctions

QnPage {
    id: loginPage

    property alias host: hostField.text
    property alias port: portField.text
    property alias login: loginField.text
    property alias password: passwordField.text
    property string sessionId: ""

    property string newConnectionLabel: qsTr("New Connection")

    property var oldClientOfferDialog: oldClientOfferDialog

    title: newConnectionLabel

    property bool _authError: false
    property bool _serverError: false

    readonly property bool connecting: connectionManager.connectionState == QnConnectionManager.Connecting

    objectName: "loginPage"

    Connections
    {
        target: menuBackButton
        onClicked:
        {
            if (state != "New")
            {
                if (connecting)
                    connectionManager.disconnectFromServer(true)
                Main.gotoMainScreen()
            }
        }
    }

    Item {
        id: warning

        property bool shown: false

        width: parent.width
        height: warningRect.height + dp(8)

        onShownChanged:
        {
            if (shown)
            {
                warningAnimation.stop()
                warningAnimation.to = warningRect.implicitHeight
                warningAnimation.start()
            }
            else
            {
                warningAnimation.stop()
                warningAnimation.to = 0
                warningAnimation.start()
            }
        }

        Rectangle {
            id: warningRect

            width: parent.width
            implicitHeight: dp(40)
            height: 0
            clip: true
            anchors.horizontalCenter: parent.horizontalCenter

            color: QnTheme.attentionBackground

            NumberAnimation on height {
                id: warningAnimation
                duration: 500
                easing.type: Easing.OutCubic
            }

            Text {
                id: warningText
                anchors.horizontalCenter: parent.horizontalCenter
                // Two lines below are the hack to prevent text from moving when the header changes its size
                anchors.verticalCenter: parent.bottom
                anchors.verticalCenterOffset: -dp(20)
                font.pixelSize: sp(16)
                font.weight: Font.DemiBold
                color: QnTheme.windowText
            }
        }
    }

    QnFlickable {
        id: flickable
        anchors.fill: parent
        anchors.topMargin: warning.height
        topMargin: dp(16)
        leftMargin: dp(16)
        rightMargin: dp(16)
        bottomMargin: dp(16)
        contentWidth: width
        contentHeight: content.height
        clip: true

        Column {
            id: content
            width: parent.width - flickable.leftMargin - flickable.rightMargin
            spacing: dp(24)

            Row {
                width: parent.width

                QnTextField {
                    id: hostField
                    width: parent.width * 3 / 5
                    placeholderText: qsTr("Host")
                    showError: _serverError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enabled: !connecting
                    activeFocusOnTab: true
                    Keys.onDownPressed: Main.focusNextItem(this)
                    Keys.onUpPressed: Main.focusPrevItem(this)
                    onAccepted: KeyNavigation.tab.forceActiveFocus()
                    onActiveFocusChanged:
                    {
                        if (activeFocus)
                            flickable.ensureItemVisible(this)
                    }
                }

                QnTextField {
                    id: portField
                    width: parent.width * 2 / 5 - parent.spacing
                    placeholderText: qsTr("Port (optional)")
                    showError: _serverError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 32767 }
                    enabled: !connecting
                    activeFocusOnTab: liteMode
                    Keys.onDownPressed: Main.focusNextItem(this)
                    Keys.onUpPressed: Main.focusPrevItem(this)
                    onAccepted: KeyNavigation.tab.forceActiveFocus()
                    onActiveFocusChanged:
                    {
                        if (activeFocus)
                            flickable.ensureItemVisible(this)
                    }
                }
            }

            Column {
                width: parent.width

                QnTextField {
                    id: loginField
                    width: parent.width
                    placeholderText: qsTr("Login")
                    showError: _authError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                    enabled: !connecting
                    activeFocusOnTab: true
                    Keys.onDownPressed: Main.focusNextItem(this)
                    Keys.onUpPressed: Main.focusPrevItem(this)
                    onAccepted: KeyNavigation.tab.forceActiveFocus()
                    onActiveFocusChanged:
                    {
                        if (activeFocus)
                            flickable.ensureItemVisible(this)
                    }
                }

                QnTextField {
                    id: passwordField
                    width: parent.width
                    placeholderText: qsTr("Password")
                    echoMode: TextInput.Password
                    passwordMaskDelay: 1500
                    showError: _authError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData | Qt.ImhHiddenText
                    enabled: !connecting
                    activeFocusOnTab: true
                    Keys.onDownPressed: Main.focusNextItem(this)
                    Keys.onUpPressed: Main.focusPrevItem(this)
                    onAccepted: connect()
                    Component.onCompleted: {
                        if (Qt.platform.os == "android")
                            passwordCharacter = "\u2022"
                    }
                    onActiveFocusChanged:
                    {
                        if (activeFocus)
                            flickable.ensureItemVisible(this)
                    }
                }
            }

            Item {
                id: buttonsItem

                width: parent.width
                height: buttonsColumn.height

                Column {
                    id: buttonsColumn

                    width: parent.width
                    spacing: dp(24)

                    Row {
                        id: editButtons
                        width: parent.width
                        spacing: dp(8)
                        visible: false

                        QnButton {
                            id: saveButton
                            text: qsTr("Save")
                            width: parent.width * 3 / 5

                            onClicked: LoginFunctions.saveSession(sessionId, hostField.text, actualPort(), loginField.text, passwordField.text, title)
                        }

                        QnButton {
                            id: deleteButton
                            text: qsTr("Delete")
                            width: parent.width * 2 / 5 - parent.spacing
                            color: QnTheme.buttonAttentionBackground

                            onClicked: LoginFunctions.deleteSesion(sessionId)
                        }
                    }

                    QnButton {
                        id: connectButton
                        width: parent.width
                        text: qsTr("Connect")
                        color: QnTheme.buttonAccentBackground
                        textColor: QnTheme.buttonAccentText

                        onClicked: connect()
                    }

                    opacity: connecting ? 0.0 : 1.0

                    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                }

                QnThreeDotPreloader {
                    height: dp(48)
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: QnTheme.preloaderCircle
                    opacity: 1.0 - buttonsColumn.opacity
                }
            }

            Loader {
                id: discoveredSessionsLoader
                width: parent.width
                enabled: !connecting
            }
        }
    }

    Component {
        id: discoveredSessionsList

        Column {
            spacing: dp(1)
            visible: discoveredSessionRepeater.count > 0

            readonly property Item sessionsRepeater: discoveredSessionRepeater

            Text {
                height: dp(40)
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Auto-discovered systems")
                color: QnTheme.listSectionText
                font.pixelSize: sp(16)
            }

            Repeater
            {
                id: discoveredSessionRepeater
                width: parent.width

                model: QnDiscoveredSessionsModel
                {
                    id: discoveredSessionsModel
                }

                QnDiscoveredSessionItem
                {
                    systemName: model.systemName
                    host: model.address
                    port: model.port
                    version: model.serverVersion
                    activeFocusOnTab: true

                    Rectangle
                    {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: QnTheme.listSelectionBorder
                        border.width: dp(4)
                        visible: parent.activeFocus
                    }

                    Keys.onReturnPressed: open()
                    Keys.onDownPressed: Main.focusNextItem(this)
                    Keys.onUpPressed: Main.focusPrevItem(this)

                    onActiveFocusChanged:
                    {
                        if (activeFocus)
                            flickable.ensureItemVisible(this)
                    }
                }
            }
        }
    }

    state: "New"

    states: [
        State {
            name: "New"
            PropertyChanges
            {
                target: discoveredSessionsLoader
                sourceComponent: discoveredSessionsList
            }
            StateChangeScript
            {
                script:
                {
                    if (liteMode)
                        hostField.forceActiveFocus()
                }
            }
        },
        State {
            name: "Discovered"
            StateChangeScript {
                script: {
                    loginField.forceActiveFocus()
                }
            }
        },
        State {
            name: "Saved"
            PropertyChanges {
                target: editButtons
                visible: true
            }
            PropertyChanges {
                target: connectButton
                visible: false
            }
        },
        State {
            name: "FailedSaved"
            PropertyChanges {
                target: editButtons
                visible: false
            }
        }
    ]

    Connections {
        target: connectionManager

        onConnectionFailed: {
            if (!activePage)
                return

            showWarning(status, infoParameter)
        }
    }

    QnOldClientOfferDialog {
        id: oldClientOfferDialog
    }

    function connect() {
        loginPage.removeWarnings()
        connectButton.forceActiveFocus()
        warning.shown = false
        loginWithCurrentFields()
    }

    function showWarning(status, infoParameter) {
        var message = ""
        if (status == QnConnectionManager.Unauthorized)
            message = qsTr("Incorrect login or password")
        else if (status == QnConnectionManager.NetworkError)
            message = qsTr("Server or network is not available")
        else if (status == QnConnectionManager.InvalidServer)
            message = qsTr("Incompatible server")
        else if (status == QnConnectionManager.InvalidVersion)
            message = qsTr("Incompatible server version %1").arg(infoParameter)

        warningText.text = message
        if (status === QnConnectionManager.Unauthorized)
            _authError = true
        else
            _serverError = true
        warning.shown = true

        if (status == QnConnectionManager.InvalidVersion)
            oldClientOfferDialog.show()
    }

    function removeWarnings() {
        _authError = false
        _serverError = false
    }

    function loginWithCurrentFields() {
        var sessionId = (state == "FailedSaved" ? loginPage.sessionId : "")
        var customConnection = (loginPage.objectName == "newConnectionPage")
        LoginFunctions.connectToServer(sessionId, hostField.text, actualPort(), loginField.text, passwordField.text, customConnection)
    }

    function actualPort() {
        return portField.text ? portField.text : connectionManager.defaultServerPort()
    }

    function scrollTop() {
        flickable.contentY = -flickable.topMargin
    }

    focus: true

    Keys.onReleased:
    {
        if (Main.keyIsBack(event.key))
        {
            if (Main.backPressed())
                event.accepted = true
        }
        else if (event.key == Qt.Key_F2)
        {
            if (state == "New")
                sideNavigation.open = !sideNavigation.open
        }
    }
}
