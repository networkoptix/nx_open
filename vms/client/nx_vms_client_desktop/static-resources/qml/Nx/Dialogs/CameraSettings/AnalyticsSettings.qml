import QtQuick 2.11
import QtQuick.Layouts 1.11
import Nx 1.0
import Nx.Utils 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0
import Nx.InteractiveSettings 1.0
import nx.vms.client.core 1.0
import Nx.Core 1.0

Item
{
    id: analyticsSettings

    property var store: null

    property var analyticsEngines: []
    property var enabledAnalyticsEngines: []

    property var currentEngineId
    property var currentEngineInfo
    property var currentSection
    property bool loading: false

    readonly property bool isDeviceDependent: currentEngineInfo !== undefined
        && currentEngineInfo.isDeviceDependent

    Connections
    {
        target: store
        onStateChanged:
        {
            const resourceId = store.resourceId()

            loading = store.analyticsSettingsLoading()
            analyticsEngines = store.analyticsEngines()
            enabledAnalyticsEngines = store.enabledAnalyticsEngines()
            if (settingsView.resourceId !== resourceId)
                settingsView.resourceId = resourceId
            mediaResourceHelper.resourceId = store.resourceId().toString()
            var engineId = store.currentAnalyticsEngineId()

            if (engineId === currentEngineId)
            {
                settingsView.setValues(store.deviceAgentSettingsValues(currentEngineId))
            }
            else if (analyticsEngines.length > 0)
            {
                var engineInfo = undefined
                for (var i = 0; i < analyticsEngines.length; ++i)
                {
                    var info = analyticsEngines[i]
                    if (info.id === engineId)
                    {
                        engineInfo = info
                        break
                    }
                }

                // Select first engine in the list if nothing selected.
                if (!engineInfo)
                {
                    engineInfo = analyticsEngines[0]
                    engineId = engineInfo.id
                }

                currentEngineId = engineId
                currentEngineInfo = engineInfo
                menu.currentItemId = engineId.toString() + currentSection
                settingsView.loadModel(
                    engineInfo.settingsModel,
                    store.deviceAgentSettingsValues(engineInfo.id))
            }
            else
            {
                currentEngineId = undefined
                currentEngineInfo = undefined
                currentSection = ""
                menu.currentItemId = undefined
                settingsView.loadModel({}, {})
            }

            banner.visible = !store.recordingEnabled() && enabledAnalyticsEngines.length !== 0
        }
    }

    LiveThumbnailProvider
    {
        id: thumbnailProvider
        rotation: 0
        thumbnailsHeight: 80
    }

    MediaResourceHelper
    {
        id: mediaResourceHelper
    }

    NavigationMenu
    {
        id: menu

        width: 240
        height: parent.height - banner.height

        Repeater
        {
            model: analyticsEngines

            Rectangle
            {
                readonly property var engineId: modelData.id
                readonly property bool isActive: enabledAnalyticsEngines.indexOf(engineId) !== -1
                readonly property bool isSelected: currentEngineId === engineId
                readonly property real textAlpha: isActive ? 1.0 : 0.3

                width: parent.width
                height: column.height
                color: isSelected ? ColorTheme.colors.dark9 : "transparent"

                Column
                {
                    id: column
                    width: parent.width

                    MenuItem
                    {
                        id: menuItem

                        height: 28
                        itemId: engineId.toString()
                        text: modelData.name
                        active: isActive
                        navigationMenu: menu

                        color: ColorTheme.transparent(
                            current || isSelected
                                ? ColorTheme.colors.light1
                                : ColorTheme.colors.light10,
                            textAlpha);

                        onClicked:
                        {
                            currentSection = ""
                            store.setCurrentAnalyticsEngineId(engineId)
                            settingsView.contentItem.sectionsItem.currentIndex = 0
                        }

                        readonly property bool collapsible:
                            !!modelData.settingsModel.sections && modelData.settingsModel.sections.length > 0

                        MouseArea
                        {
                            id: expandCollapseButton

                            width: 20
                            height: parent.height
                            anchors.right: parent.right
                            acceptedButtons: Qt.LeftButton
                            visible: parent.collapsible
                            hoverEnabled: true

                            ArrowIcon
                            {
                                anchors.centerIn: parent
                                rotation: sectionsView.collapsed ? 0 : 180
                                color: expandCollapseButton.containsMouse && !expandCollapseButton.pressed
                                    ? ColorTheme.lighter(menuItem.color, 2)
                                    : menuItem.color
                            }

                            onClicked:
                            {
                                if (isSelected)
                                    parent.click()

                                sectionsView.collapsed = !sectionsView.collapsed
                            }
                        }
                    }

                    // Only 1 level of submenu items is supported at the moment.
                    Column
                    {
                        id: sectionsView
                        width: parent.width
                        clip: true

                        property bool collapsed: false
                        height: collapsed ? 0 : implicitHeight

                        Repeater
                        {
                            model: modelData.settingsModel.sections

                            MenuItem
                            {
                                height: 28
                                itemId: engineId.toString() + modelData.name
                                text: modelData.name
                                leftPadding: 24
                                navigationMenu: menu
                                font.pixelSize: 12
                                active: isActive

                                color: ColorTheme.transparent(
                                    current ? ColorTheme.colors.light1 : ColorTheme.colors.light10,
                                    textAlpha);

                                onClicked:
                                {
                                    currentSection = modelData.name
                                    store.setCurrentAnalyticsEngineId(engineId)
                                    settingsView.contentItem.sectionsItem.currentIndex = index + 1
                                }
                            }
                        }
                    }

                    Rectangle
                    {
                        id: separator

                        width: parent.width
                        height: 1
                        color: ColorTheme.colors.dark7

                        Rectangle
                        {
                            y: 1
                            z: 1
                            width: parent.width
                            height: 1
                            color: ColorTheme.colors.dark9
                        }
                    }
                }
            }
        }
    }

    ColumnLayout
    {
        x: menu.width + 16
        y: 16
        width: parent.width - x - 16
        height: parent.height - 16 - banner.height
        spacing: 16

        enabled: !loading

        RowLayout
        {
            visible: currentEngineId !== undefined && isDeviceDependent
            spacing: 16

            Image
            {
                source: "qrc:/skin/standard_icons/sp_message_box_information.png"
            }

            Text
            {
                wrapMode: Text.WordWrap
                color: ColorTheme.windowText
                font.pixelSize: 13
                font.weight: Font.Bold
                text: qsTr("This is the built-in functionality")
            }
        }

        SwitchButton
        {
            id: enableSwitch
            text: qsTr("Enable")
            Layout.preferredWidth: Math.max(implicitWidth, 120)
            visible: currentEngineId !== undefined && !isDeviceDependent && currentSection == ""

            Binding
            {
                target: enableSwitch
                property: "checked"
                value: enabledAnalyticsEngines.indexOf(currentEngineId) !== -1
                when: currentEngineId !== undefined
            }

            onClicked:
            {
                var engines = enabledAnalyticsEngines.slice(0)
                if (checked)
                {
                    engines.push(currentEngineId)
                }
                else
                {
                    const index = engines.indexOf(currentEngineId)
                    if (index !== -1)
                        engines.splice(index, 1)
                }
                store.setEnabledAnalyticsEngines(engines)
            }
        }

        SettingsView
        {
            id: settingsView

            Layout.fillHeight: true
            Layout.fillWidth: true

            onValuesEdited: { store.setDeviceAgentSettingsValues(currentEngineId, getValues()) }

            contentEnabled: enableSwitch.checked
            scrollBarParent: scrollBarsParent
        }
    }

    Item
    {
        id: scrollBarsParent
        width: parent.width
        height: banner.y
    }

    Banner
    {
        id: banner

        height: visible ? implicitHeight : 0
        visible: false
        anchors.bottom: parent.bottom
        text: qsTr("Camera analytics will work only when camera is being viewed."
            + " Enable recording to make it work all the time.")
    }
}
