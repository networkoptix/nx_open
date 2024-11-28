// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window
import Nx.Core

import nx.vms.client.core

ShaderEffect
{
    /* Source parameters: */
    property alias sourceItem: shaderSource.sourceItem

    readonly property size sourceSize: sourceItem
        ? Qt.size(sourceItem.implicitWidth, sourceItem.implicitHeight)
        : Qt.size(0.0, 0.0)

    /* Equidistant projection is the most common among lenses, but it requires 32-bit float
     * precision support on fragment shader to work well (due to low precision trigonometry).
     * Therefore we choose somewhat similar equisolid projection as default. */
    property int lensProjectionType: Utils3D.SphereProjectionTypes.Equisolid
    property int viewProjectionType: lensProjectionType

    blending: false

    /* Fisheye parameters: */

    property real fieldRadius: Math.min(sourceSize.width, sourceSize.height) / (sourceSize.width * 2.0)
    property vector2d fieldOffset: Qt.vector2d(0.0, 0.0)
    property real fieldStretch: 1.0
    property real fieldRotation: 0.0

    /* Camera parameters: */

    property real viewScale: 1.0
    property matrix4x4 viewRotationMatrix
    property vector2d viewShift

    /* Shader uniforms: */

    TextureSizeHelper
    {
        id: textureSizeHelper
    }

    ShaderEffectSource
    {
        id: shaderSource
        hideSource: true
        visible: false
        textureSize: Qt.size(
            Math.min(parent.sourceSize.width, textureSizeHelper.maxTextureSize),
            Math.min(parent.sourceSize.height, textureSizeHelper.maxTextureSize))
    }

    readonly property var sourceTexture: shaderSource

    readonly property vector2d projectionCoordsScale: calculateProjectionScale(viewScale)

    function calculateProjectionScale(viewScale)
    {
        var targetAspectRatio = width / height
        return (targetAspectRatio < 1.0
            ? Qt.vector2d(1.0, 1.0 / targetAspectRatio)
            : Qt.vector2d(targetAspectRatio, 1.0)).times(2.0 / viewScale)
    }

    readonly property matrix4x4 textureMatrix: // maps hemispere projection coords into texture coords
    {
        var sourceAspectRatio = sourceSize.width / sourceSize.height
        const stretch = isNaN(fieldStretch) ? 1.0 : fieldStretch
        const textureCoordsScale = Qt.vector2d(1.0, stretch / sourceAspectRatio).times(1.0 / fieldRadius)
        var textureCoordsCenter = Qt.vector2d(0.5, 0.5).minus(fieldOffset)

        return Utils3D.translation(textureCoordsCenter.x, textureCoordsCenter.y, 0.0).times(
               Utils3D.scaling(1.0 / textureCoordsScale.x, 1.0 / textureCoordsScale.y, 1.0)).times(
               Utils3D.rotationZ(Utils3D.radians(-fieldRotation)))
    }

    readonly property vector2d viewCenter: Qt.vector2d(0.5, 0.5).plus(viewShift)

    vertexShader: "qrc:/qml/Nx/Screens/private/VideoScreen/FisheyeShaderEffect.vert.qsb"
    fragmentShader: `qrc:/qml/Nx/Screens/private/VideoScreen/FisheyeShaderEffect_${projectionName(lensProjectionType)}_${projectionName(viewProjectionType)}.frag.qsb`

    function projectionName(projectionType)
    {
        switch (projectionType)
        {
            case Utils3D.SphereProjectionTypes.Stereographic:
                return "Stereographic"
                break;

            case Utils3D.SphereProjectionTypes.Equisolid:
                return "Equisolid"

            default: // Utils3D.SphereProjectionTypes.Equidistant
                return "Equidistant"
        }
    }

    function fov(scale) //< Field of view in degrees.
    {
        switch (viewProjectionType)
        {
            case Utils3D.SphereProjectionTypes.Equidistant:
                return 180.0 / scale

            case Utils3D.SphereProjectionTypes.Equisolid:
                return 4.0 * Utils3D.degrees(Math.asin(1.0 / (scale * Math.sqrt(2.0))))

            default: // Utils3D.SphereProjectionTypes.Stereographic
                return 4.0 * Utils3D.degrees(Math.atan(1.0 / scale))
        }
    }

    function pixelToProjection(x, y, scale)
    {
        return Qt.vector2d(x / width, y / height).minus(viewCenter)
            .times(calculateProjectionScale(scale))
    }

    function unproject(projectionCoords)
    {
        switch (viewProjectionType)
        {
            case Utils3D.SphereProjectionTypes.Equidistant:
            {
                var r = Math.min(projectionCoords.length(), 2.0)
                var theta = r * Math.PI / 2.0
                var xy = projectionCoords.times(Math.sin(theta) / r)
                return Qt.vector3d(xy.x, xy.y, -Math.cos(theta))
            }

            case Utils3D.SphereProjectionTypes.Equisolid:
            {
                var r2 = Math.min(projectionCoords.dotProduct(projectionCoords), 2.0)
                var xy = projectionCoords.times(Math.sqrt(2.0 - r2))
                return Qt.vector3d(xy.x, xy.y, r2 - 1.0)
            }

            default: // Utils3D.SphereProjectionTypes.Stereographic
            {
                var r2 = projectionCoords.dotProduct(projectionCoords)
                var xy = projectionCoords.times(2.0)
                return Qt.vector3d(xy.x, xy.y, r2 - 1.0).times(1.0 / (r2 + 1.0))
            }
        }
    }
}
