// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

Text
{
    property string graphicsInfo: ""

    text: performanceInfo.text + `<br>Graphics: ${graphicsInfo}`
    textFormat: Text.RichText
    color: ColorTheme.highlight
    font.pixelSize: FontConfig.normal.pixelSize

    visible: performanceInfo.visible

    property var previousWindow: null

    onWindowChanged: function(window)
    {
        // Avoid counting frame renderings from previous window (if any).
        if (previousWindow)
            previousWindow.beforeRendering.disconnect(performanceInfo.registerFrame)

        if (window)
            window.beforeRendering.connect(performanceInfo.registerFrame)

        previousWindow = window
        updateGraphicsInfo()
    }

    Connections
    {
        target: previousWindow
        function onSceneGraphInitialized() { updateGraphicsInfo() }
        function onSceneGraphInvalidated() { updateGraphicsInfo() }
    }

    function updateGraphicsInfo()
    {
        graphicsInfo = getGraphicsInfo()
    }

    function getGraphicsInfo()
    {
        const driverInfo = NxGlobals.getDriverInfo(previousWindow)
        let gpuName = driverInfo ? `${driverInfo.deviceName}` : ""
        // Truncate GPU name so the the text doesn't appear too wide.
        const kMaxGpuNameLength = 15
        if (gpuName.length > kMaxGpuNameLength + 3)
            gpuName = gpuName.substring(0, kMaxGpuNameLength) + '...'
        if (gpuName.length > 0)
            gpuName = `(${gpuName})`

        switch (GraphicsInfo.api)
        {
            case GraphicsInfo.Software:
                return "Software";
            case GraphicsInfo.OpenGL:
            {
                const name = GraphicsInfo.renderableType == GraphicsInfo.SurfaceFormatOpenGLES
                    ? "OpenGL ES"
                    : "OpenGL"
                const profile = GraphicsInfo.profile == GraphicsInfo.OpenGLCoreProfile
                    ? "Core"
                    : "Compat"
                const version = `${GraphicsInfo.majorVersion}.${GraphicsInfo.minorVersion}`
                return `${name} ${version} ${profile} ${gpuName}`;
            }
            case GraphicsInfo.Direct3D11:
                return `Direct3D 11 ${gpuName}`;
            case GraphicsInfo.Direct3D12:
                return `Direct3D 12 ${gpuName}`;
            case GraphicsInfo.Vulkan:
                return `Vulkan ${gpuName}`;
            case GraphicsInfo.Metal:
                return "Metal";
            default:
                return `Unknown ${GraphicsInfo.api}`;
        }
    }

    PerformanceInfo
    {
        id: performanceInfo
    }
}
