// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Models 1.0

Item
{
    id: openedTileItem

    readonly property int requiredWidth: column.x * 2 + Math.max(
        hostChooseItem.requiredContentWidth,
        connectErrorItem.visible ? connectErrorItem.contentWidth : 0,
        loginChooseItem.requiredContentWidth,
        passwordItem.contentWidth + passwordItem.leftPadding + passwordItem.rightPadding,
        loginErrorItem.visible ? loginErrorItem.contentWidth : 0)

    property SystemHostsModel systemHostsModel: null
    property AuthenticationDataModel authenticationDataModel: null

    property bool isConnecting: false
    property bool isPasswordSaved: false

    property string errorMessage: ""
    property bool isLoginError: false

    property string hostText: ""
    property int hostIndex: -1

    property alias user: loginChooseItem.login
    property alias password: passwordItem.text
    property alias savePassword: savePasswordCheckbox.checked

    readonly property alias tileNameRightPadding: closeButton.width

    signal connect
    signal shrink
    signal abortConnectionProcess
    signal savedPasswordCleared

    function setError(errorMessage, isLoginError)
    {
        impl.updatePasswordData()
        passwordItem.forceActiveFocus()
        openedTileItem.isLoginError = isLoginError
        openedTileItem.errorMessage = errorMessage
    }

    function handleKeyPressed(event)
    {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
        {
            event.accepted = true

            if (hostText && user && (password || isPasswordSaved))
                connect()
        }
    }

    height: column.y + column.height + 16

    onVisibleChanged:
    {
        if (!visible)
            return

        if (!host)
            hostChooseItem.forceActiveFocus()
        else if (!user)
            loginChooseItem.forceActiveFocus()
        else if (!password && !isPasswordSaved)
            passwordItem.forceActiveFocus()
        else
            connectButton.forceActiveFocus()
    }

    onIsConnectingChanged:
    {
        if (!isConnecting && !isPasswordSaved)
            passwordItem.forceActiveFocus()
    }

    Button
    {
        id: closeButton

        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        anchors.right: parent.right
        width: 40
        height: 40

        flat: true
        backgroundColor: "transparent"
        pressedColor: ColorTheme.colors.dark6
        hoveredColor: ColorTheme.colors.dark9
        outlineColor: "transparent"

        iconUrl: "image://svg/skin/welcome_screen/tile/close.svg"
        iconWidth: 20
        iconHeight: 20

        onClicked:
        {
            openedTileItem.shrink()
            if (openedTileItem.isConnecting)
                openedTileItem.abortConnectionProcess()
        }

        KeyNavigation.backtab: connectButton
    }

    Column
    {
        id: column

        x: 16
        anchors.top: closeButton.bottom
        anchors.topMargin: 16

        spacing: 16

        clip: true

        Column
        {
            spacing: 8

            Label
            {
                height: 16
                font.pixelSize: 14
                color: ColorTheme.windowText

                text: qsTr("Address")
            }

            ComboBox
            {
                id: hostChooseItem

                readonly property bool warningState: !openedTileItem.isLoginError
                    && openedTileItem.errorMessage

                width: openedTileItem.width - column.x * 2
                height: 28

                model: openedTileItem.systemHostsModel

                placeholderText: qsTr("URL or Host:Port")
                textRole: "display"
                editable: true

                enabled: !openedTileItem.isConnecting

                onEditTextChanged:
                {
                    openedTileItem.hostText = editText
                    openedTileItem.hostIndex = editText != displayText ? -1 : currentIndex
                }

                onCurrentIndexChanged:
                {
                    openedTileItem.hostText = editText
                    openedTileItem.hostIndex = currentIndex
                }

                onCountChanged:
                {
                    if (count > 0 && currentIndex === -1)
                        currentIndex = 0
                }

                Keys.onPressed: event => openedTileItem.handleKeyPressed(event)
            }

            Label
            {
                id: connectErrorItem

                width: hostChooseItem.width
                height: 16

                font.pixelSize: 14
                color: ColorTheme.colors.red_l2

                visible: hostChooseItem.warningState

                text: openedTileItem.errorMessage
            }
        }

        Column
        {
            spacing: 8

            Label
            {
                height: 16
                font.pixelSize: 14
                color: ColorTheme.windowText

                text: qsTr("Login")
            }

            ComboBox
            {
                id: loginChooseItem

                readonly property string login: editText.trim()

                readonly property bool warningState: openedTileItem.isLoginError
                    && openedTileItem.errorMessage

                readonly property int editIndex: find(login)

                function set(login)
                {
                    currentIndex = find(login)
                    if (currentIndex == -1)
                        editText = login
                }

                width: openedTileItem.width - column.x * 2
                height: 28

                model: authenticationDataModel
                textRole: "display"
                editable: true
                enabled: !openedTileItem.isConnecting

                Keys.onPressed: event => openedTileItem.handleKeyPressed(event)
            }
        }

        Column
        {
            spacing: 8

            Label
            {
                height: 16
                font.pixelSize: 14
                color: ColorTheme.windowText

                text: qsTr("Password")
            }

            Control
            {
                width: openedTileItem.width - column.x * 2
                height: 28
                rightPadding: clearPasswordButton.width + 1

                TextField
                {
                    id: passwordItem

                    readonly property bool warningState: loginChooseItem.warningState

                    anchors.fill: parent
                    echoMode: TextInput.Password
                    hidePlaceholderOnFocus: true
                    placeholderTextColor: color
                    placeholderText: isPasswordSaved
                        ? "\u25cf\u25cf\u25cf\u25cf\u25cf\u25cf\u25cf\u25cf"
                        : ""

                    enabled: !openedTileItem.isConnecting && !isPasswordSaved

                    onTextChanged: openedTileItem.errorMessage = ""

                    Keys.onPressed: event => openedTileItem.handleKeyPressed(event)

                    KeyNavigation.tab: savePasswordCheckbox
                }

                Button
                {
                    id: clearPasswordButton

                    leftPadding: 0
                    rightPadding: 0
                    topPadding: 0
                    bottomPadding: 0

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 1
                    width: height
                    height: parent.height - 2

                    flat: true
                    backgroundColor: ColorTheme.colors.dark5
                    iconUrl: "image://svg/skin/welcome_screen/cross.svg"
                    iconWidth: 20
                    iconHeight: 20
                    activeFocusOnTab: false

                    enabled: !openedTileItem.isConnecting
                    visible: passwordItem.text || isPasswordSaved

                    onClicked:
                    {
                        if (isPasswordSaved)
                        {
                            savedPasswordCleared()
                            impl.updatePasswordData()
                        }

                        passwordItem.forceActiveFocus()
                        passwordItem.clear()
                    }
                }
            }

            Label
            {
                id: loginErrorItem

                width: loginChooseItem.width
                height: 16

                textFormat: Text.StyledText
                font.pixelSize: 14
                color: ColorTheme.colors.red_l2
                linkColor: color

                visible: loginChooseItem.warningState

                text: openedTileItem.errorMessage

                onLinkActivated:
                {
                    if (link === "#cloud")
                    {
                        context.loginToCloud()
                        openedTileItem.shrink()
                    }
                }

                onLinkHovered: linkColor = link ? ColorTheme.colors.red_l3 : color
            }

            CheckBox
            {
                id: savePasswordCheckbox

                text: qsTr("Remember me")

                KeyNavigation.tab: connectButton

                Keys.onPressed: event => openedTileItem.handleKeyPressed(event)
            }
        }

        Button
        {
            id: connectButton

            width: openedTileItem.width - column.x * 2

            isAccentButton: true
            text: !openedTileItem.isConnecting ? qsTr("Connect") : ""
            hoverEnabled: !openedTileItem.isConnecting

            enabled: openedTileItem.hostText
                && openedTileItem.user
                && (openedTileItem.password || openedTileItem.isPasswordSaved)

            KeyNavigation.tab: closeButton

            onClicked:
            {
                openedTileItem.errorMessage = ""
                openedTileItem.connect()
            }

            // MouseArea disables button without changing its visual state.
            MouseArea
            {
                anchors.fill: parent
                visible: openedTileItem.isConnecting
            }

            NxDotPreloader
            {
                color: ColorTheme.brightText
                running: openedTileItem.isConnecting

                anchors.centerIn: parent
            }
        }
    }

    property var impl: NxObject
    {
        readonly property bool hasRecentConnections: authDataAccessor.count > 0
        property string lastLogin: ""

        ModelDataAccessor
        {
            id: authDataAccessor
            model: openedTileItem.authenticationDataModel

            onDataChanged: (startRow, endRow) =>
            {
                if (startRow !== loginChooseItem.currentIndex)
                    return //< Do not update if it is not current item.

                impl.selectLastLogin()
            }

            onCountChanged: impl.selectLastLogin()
            onModelChanged: impl.selectLastLogin()
        }

        Connections
        {
            target: loginChooseItem
            enabled: !isConnecting

            function onEditTextChanged()
            {
                impl.lastLogin = loginChooseItem.login
            }

            function onEditIndexChanged()
            {
                impl.updatePasswordData()
                openedTileItem.password = ""
                openedTileItem.errorMessage = ""
            }
        }

        function updatePasswordData()
        {
            const index = loginChooseItem.editIndex
            const credentials = authDataAccessor.getData(index, "credentials")

            const isPasswordSaved = context.saveCredentialsAllowed
                && credentials !== undefined
                && credentials.isPasswordSaved
                && credentials.user.toLowerCase() === loginChooseItem.login.toLowerCase()

            const isSavedPasswordChanged = isPasswordSaved != openedTileItem.isPasswordSaved
            if (isSavedPasswordChanged)
            {
                openedTileItem.isPasswordSaved = isPasswordSaved
                openedTileItem.savePassword = isPasswordSaved
                openedTileItem.password = ""
            }
        }

        function selectLastLogin()
        {
            loginChooseItem.set(lastLogin)

            if (!isConnecting)
                updatePasswordData()
        }

        Component.onCompleted:
        {
            loginChooseItem.currentIndex = 0
            lastLogin = loginChooseItem.currentText
        }
    }
}
