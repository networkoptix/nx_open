// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

/**
 * Produces linear opacity gradient at specified edges.
 */
ShaderEffect
{
    id: effect

    property int edges: Qt.LeftEdge | Qt.RightEdge | Qt.TopEdge | Qt.BottomEdge

    property real gradientWidth: 28

    property real leftGradientWidth: gradientWidth
    property real rightGradientWidth: gradientWidth
    property real topGradientWidth: gradientWidth
    property real bottomGradientWidth: gradientWidth

    readonly property vector4d thresholds: Qt.vector4d(
        (edges & Qt.LeftEdge) ? (leftGradientWidth / width) : 0.0,
        (edges & Qt.TopEdge) ? (topGradientWidth / height) : 0.0,
        (edges & Qt.RightEdge) ? (rightGradientWidth / width) : 0.0,
        (edges & Qt.BottomEdge) ? (bottomGradientWidth / height) : 0.0)

    fragmentShader: "
        uniform lowp sampler2D source;
        uniform lowp float qt_Opacity; //< Inherited opacity.
        uniform mediump vec4 thresholds;
        varying highp vec2 qt_TexCoord0;

        void main()
        {
            // Opacity per edge proximity, left-top-right-bottom.
            mediump vec4 opacities = vec4(1.0) - max((vec4(1.0) - vec4(qt_TexCoord0, vec2(1.0)
                - qt_TexCoord0) / thresholds), vec4(0.0));

            // Final opacity is a product of all four.
            mediump float opacity = opacities.s * opacities.t * opacities.p * opacities.q;

            lowp vec4 rgba = texture2D(source, qt_TexCoord0);
            gl_FragColor = rgba * (qt_Opacity * opacity);
        }"
}
