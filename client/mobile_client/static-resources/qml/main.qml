import QtQuick 2.0
import QtQuick.Window 2.2
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

ApplicationWindow
{
    id: mainWindow

    property real leftPadding: 0
    property real rightPadding: 0
    property real bottomPadding: 0
    readonly property real keyboardHeight: Qt.inputMethod.visible
        ? Qt.inputMethod.keyboardRectangle.height
            / (Qt.platform.os !== "ios" ? Screen.devicePixelRatio : 1)
        : 0

    readonly property real availableWidth: width - rightPadding - leftPadding
    readonly property real availableHeight: height - bottomPadding

    contentItem.x: leftPadding

    visible: true
    color: ColorTheme.windowBackground
    overlay.background: Rectangle
    {
        color: ColorTheme.transparent(ColorTheme.base5, 0.4)
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    SideNavigation { id: sideNavigation }

    StackView
    {
        id: stackView

        width: mainWindow.availableWidth
        height: mainWindow.availableHeight - keyboardHeight

        property real keyboardHeight: mainWindow.keyboardHeight
        Behavior on keyboardHeight
        {
            enabled: Qt.platform.os === "android"
            NumberAnimation { duration: 200; easing.type: Easing.InCubic }
        }

        function restoreActiveFocus()
        {
            if (activeFocusItem == Window.contentItem)
                Workflow.focusCurrentScreen()
        }

        onCurrentItemChanged: sideNavigation.close()
        onWidthChanged: autoScrollDelayTimer.restart()
        onHeightChanged: autoScrollDelayTimer.restart()
    }

    UiController {}

    Loader { id: testLoader }

    Component.onCompleted:
    {
        updateNavigationBarPadding()

        if (autoLoginEnabled)
        {
            var url = getInitialUrl()
            var systemName = ""

            if (url == "")
            {
                url = getLastUsedUrl()
                systemName = getLastUsedSystemName()
            }

            if (url != "")
            {
                Workflow.openResourcesScreen(systemName)
                connectionManager.connectToServer(url)
                return
            }
        }

        // TODO: #dklychkov Check if need it in #3.1 and uncomment.
        // if (cloudStatusWatcher.status == QnCloudStatusWatcher.LoggedOut)
        // {
        //     Workflow.openCloudWelcomeScreen()
        //     return
        // }

        Workflow.openSessionsScreen()

        if (initialTest)
            Workflow.startTest(initialTest)
    }

    Component.onDestruction:
    {
        connectionManager.disconnectFromServer()
    }

    function updateNavigationBarPadding()
    {
        if (Qt.platform.os !== "android")
            return

        if (!getDeviceIsPhone() || Screen.orientation === Qt.PortraitOrientation
            || Screen.orientation === Qt.InvertedPortraitOrientation)
        {
            rightPadding = 0
            leftPadding = 0
            bottomPadding = getNavigationBarHeight()
            return
        }

        var leftSideNavigationBar = getNavigationBarIsLeftSide()
        leftPadding = leftSideNavigationBar ? getNavigationBarHeight() : 0
        rightPadding = leftSideNavigationBar ? 0 : getNavigationBarHeight()
        bottomPadding = 0
    }

    Screen.orientationUpdateMask:
        Qt.LandscapeOrientation | Qt.InvertedLandscapeOrientation
        | Qt.PortraitOrientation | Qt.InvertedPortraitOrientation

    Timer
    {
        // We have to use this workaround timer because of orientation changed
        // notification order. Without it at time when we update navigation bar
        // paddings we have old values for window insets.
        id: androidOrientationChangedWorkaroundTimer
        interval: 100

        onTriggered: updateNavigationBarPadding()
    }

    Screen.onOrientationChanged:
    {
        if (Qt.platform.os == "android")
            androidOrientationChangedWorkaroundTimer.restart()
        else
            updateNavigationBarPadding()
    }

    onActiveFocusItemChanged:
    {
        autoScrollDelayTimer.restart()
        stackView.restoreActiveFocus()
    }

    Timer
    {
        id: autoScrollDelayTimer
        interval: 50
        onTriggered: NxGlobals.ensureFlickableChildVisible(activeFocusItem)
    }
}
