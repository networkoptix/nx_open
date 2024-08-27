// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

Text
{
    readonly property string graphicsApiName: getApiName()
    text: performanceInfo.text + `<br>Graphics API: ${graphicsApiName}`
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
    }

    function getApiName()
    {
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
                    : "Compatibility"
                const version = `${GraphicsInfo.majorVersion}.${GraphicsInfo.minorVersion}`
                return `${name} ${version} ${profile}`;
            }
            case GraphicsInfo.Direct3D11:
                return "Direct3D 11";
            case GraphicsInfo.Vulkan:
                return "Vulkan";
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
