import QtQuick 2.6

ShaderEffect
{
    property alias sourceItem: shaderSource.sourceItem

    readonly property size sourceSize: sourceItem 
        ? Qt.size(sourceItem.width, sourceItem.height)
        : Qt.size(0.0, 0.0)

    readonly property size targetSize: Qt.size(width, height)

    blending: false

    /* Fisheye parameters: */

    property real radius: Math.min(sourceSize.width, sourceSize.height) / sourceSize.width
    property vector2d offset: Qt.vector2d(0.0, 0.0)
    property real horizontalStretch: 1.0
    property real rotation: 0.0 //TODO: #vkutin Use it

//TODO: #vkutin This will be used in actual dewarping
//  property real viewFov
//  property point viewCenter

    /* Vertex shader uniforms - fisheye field ellipse mapping: */

    readonly property vector2d texCoordScale:
    {
        var sourceAspectRatio = sourceSize.width / sourceSize.height;
        var targetAspectRatio = targetSize.width / targetSize.height;

        var diameter = radius * 2.0;
        var mapFromEllipse = Qt.vector2d(1.0 / diameter, horizontalStretch / (diameter * sourceAspectRatio))
 
        return targetAspectRatio < 1.0
            ? Qt.vector2d(1.0, targetAspectRatio).times(mapFromEllipse)
            : Qt.vector2d(1.0 / targetAspectRatio, 1.0).times(mapFromEllipse)
    }

    readonly property vector2d texCoordCenter: Qt.vector2d(0.5, 0.5).plus(offset)

    /* Vertex shader code: */

    vertexShader: "
        uniform mat4 qt_Matrix;
        uniform vec2 texCoordScale;
        uniform vec2 texCoordCenter; // ellipse center

        attribute vec4 qt_Vertex;
        attribute vec2 qt_MultiTexCoord0;

        varying vec2 textureCoord;

        void main()
        {
            textureCoord = vec2(0.5, 0.5) + (qt_MultiTexCoord0 - texCoordCenter) / texCoordScale;
            gl_Position = qt_Matrix * qt_Vertex;
        }"

    /* Fragment shader uniforms: */

    ShaderEffectSource
    {
        id: shaderSource
        hideSource: true
        format: ShaderEffectSource.RGB
        visible: false
    }

    readonly property var sourceTexture: shaderSource

    /* Fragment shader code: */

    fragmentShader: "
        varying vec2 textureCoord;

        uniform sampler2D sourceTexture;
        uniform float qt_Opacity;

        vec4 texture2DBlackBorder(sampler2D sampler, vec2 coord)
        {
             bool isInside = coord.x >= 0.0 && coord.x <= 1.0 
                          && coord.y >= 0.0 && coord.y <= 1.0;

             /* Turn outside areas to black without conditional operator: */
             return texture2D(sampler, coord) * vec4(vec3(float(isInside)), 1.0);
        }

        //TODO: #FIXME #vkutin Stub: plain texture lookup
        void main() 
        {
             gl_FragColor = texture2DBlackBorder(sourceTexture, textureCoord);
        }"
}
