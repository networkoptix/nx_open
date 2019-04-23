import QtQuick 2.11
import Nx 1.0
import Nx.Utils 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0

Item
{
    id: pluginsInformation

    property var store: null

    property var plugins: []
    property var currentPlugin
    property var currentPluginDetails

    property bool online: false
    property bool loading: false
    property bool hasPlugins: plugins && plugins.length > 0

    Item 
    {
        visible: online && !loading && hasPlugins
        anchors.fill: parent

        Connections
        {
            target: store
            onStateChanged:
            {
                online = store.isOnline()
                loading = store.pluginsInformationLoading()
                plugins = store.pluginModules()
                menu.currentItemId = null
                currentPlugin = store.currentPlugin()
                currentPluginDetails = store.currentPluginDetails()
            }
        }

        NavigationMenu
        {
            id: menu

            width: 240
            height: parent.height

            Repeater
            {
                model: plugins

                MenuItem
                {
                    id: thisItem
                    text: modelData.name
                    onClicked: store.selectCurrentPlugin(modelData.libraryName)

                    Connections
                    {
                        target: pluginsInformation
                        onCurrentPluginChanged:
                        {
                            if (currentPlugin && modelData.libraryName == currentPlugin.libraryName)
                                menu.currentItemId = thisItem
                        }
                    }
                }
            }
        }

        Panel
        {
            anchors.fill: parent
            anchors.margins: 16
            anchors.leftMargin: menu.width + 16
            spacing: 16
            visible: currentPlugin !== undefined
            title: currentPlugin ? currentPlugin.name : ""

            Text
            {
                id: descriptionText
                width: parent.width
                text: currentPlugin ? currentPlugin.description : ""
                wrapMode: Text.Wrap
                color: ColorTheme.windowText
                topPadding: 8
                bottomPadding: 8
            }

            Row
            {
                width: parent.width
                spacing: 16
                anchors.top: descriptionText.bottom
                anchors.topMargin: 8

                Column
                {       
                    width: Math.max(implicitWidth, 64)
    
                    Repeater
                    {
                        model: currentPluginDetails

                        Text
                        {
                            text: modelData.name
                            color: ColorTheme.windowText
                            height: 20
                        }
                    }
                }

                Column
                {           
                    Repeater
                    {
                        model: currentPluginDetails

                        Text
                        {
                            text: modelData.value
                            color: ColorTheme.light
                            height: 20
                        }
                    }
                }
            }
        }
    }

    Text
    {
        id: noPluginsMessage

        anchors.centerIn: parent
        visible: online && !loading && !hasPlugins
        text: qsTr("No plugins installed")
        color: ColorTheme.light
        font.weight: Font.Light
        font.pixelSize: 20
    }

    Text
    {
        id: offlineMessage

        anchors.centerIn: parent
        visible: !online
        text: qsTr("Server is offline")
        color: ColorTheme.light
        font.weight: Font.Light
        font.pixelSize: 20
    }

    NxDotPreloader
    {
        id: loadingIndicator

        visible: online && loading
        anchors.centerIn: parent
        color: ColorTheme.windowText
    }
}
