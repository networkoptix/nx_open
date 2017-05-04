import QtQuick 2.6
import Nx 1.0

ShaderEffect
{
    /* Source parameters: */
    property alias sourceItem: shaderSource.sourceItem

    readonly property size sourceSize: sourceItem 
        ? Qt.size(sourceItem.width, sourceItem.height)
        : Qt.size(0.0, 0.0)

    property int lensProjectionType: Utils3D.SphereProjectionTypes.Equidistant
    property int viewProjectionType: lensProjectionType // Utils3D.SphereProjectionTypes.Stereographic

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

    /* Feedback parameters (to be queried from outside): */

    readonly property real fov: // field of view in degrees
    {
        switch (viewProjectionType)
        {
            case Utils3D.SphereProjectionTypes.Equidistant:
                return 180.0 / viewScale

            case Utils3D.SphereProjectionTypes.Equisolid:
                return 4.0 * Utils3D.degrees(Math.asin(1.0 / (viewScale * Math.sqrt(2.0))))

            default: // Utils3D.SphereProjectionTypes.Stereographic
                return 4.0 * Utils3D.degrees(Math.atan(1.0 / viewScale))
        }
    }

    /* Shader uniforms: */

    ShaderEffectSource
    {
        id: shaderSource
        hideSource: true
        format: ShaderEffectSource.RGB
        visible: false
    }

    readonly property var sourceTexture: shaderSource

    readonly property vector2d projectionCoordsScale:
    {
        var targetAspectRatio = width / height
        return (targetAspectRatio < 1.0
            ? Qt.vector2d(1.0, 1.0 / targetAspectRatio)
            : Qt.vector2d(targetAspectRatio, 1.0)).times(2.0 / viewScale)
    }

    readonly property matrix4x4 textureMatrix: // maps hemispere projection coords into texture coords
    {
        var sourceAspectRatio = sourceSize.width / sourceSize.height
        var textureCoordsScale = Qt.vector2d(1.0, fieldStretch / sourceAspectRatio).times(1.0 / fieldRadius)
        var textureCoordsCenter = Qt.vector2d(0.5, 0.5).minus(fieldOffset)

        return Utils3D.translation(textureCoordsCenter.x, textureCoordsCenter.y, 0.0).times(
               Utils3D.scaling(1.0 / textureCoordsScale.x, 1.0 / textureCoordsScale.y, 1.0)).times(
               Utils3D.rotationZ(Utils3D.radians(-fieldRotation)))
    }

    readonly property vector2d viewCenter: Qt.vector2d(0.5, 0.5).plus(viewShift)

    /* Vertex shader code: */

    vertexShader: "
        #version 110
        uniform vec2 projectionCoordsScale;
        uniform vec2 viewCenter;
        uniform float viewScale;
        uniform mat4 qt_Matrix;

        attribute vec4 qt_Vertex;
        attribute vec2 qt_MultiTexCoord0;

        varying vec2 projectionCoords; // carthesian coords local to hemispere projection unit circle

        void main()
        {
            projectionCoords = (qt_MultiTexCoord0 - viewCenter) * projectionCoordsScale;
            gl_Position = qt_Matrix * qt_Vertex;
        }"

    /* Fragment shader code: */

    fragmentShader: "
        #version 110
        varying vec2 projectionCoords;

        uniform mat4 textureMatrix;
        uniform mat4 viewRotationMatrix;
        uniform sampler2D sourceTexture;
        uniform float qt_Opacity;

        const float pi = 3.1415926;

        vec4 texture2DBlackBorder(sampler2D sampler, vec2 coord)
        {
             /* Turn outside areas to black without conditional operator: */
             bool isInside = all(bvec4(coord.x >= 0.0, coord.x <= 1.0, coord.y >= 0.0, coord.y <= 1.0));
             return texture2D(sampler, coord) * vec4(vec3(float(isInside)), 1.0);
        }

        /* Projection functions should be written with the following consideration:
         *  a unit hemisphere is projected into a unit circle. */
        vec2 project(vec3 coords); 
        vec3 unproject(vec2 coords);

        void main() 
        {
             vec4 pointOnSphere = vec4(unproject(projectionCoords), 1.0);
             vec2 transformedProjectionCoords = project((viewRotationMatrix * pointOnSphere).xyz);
             vec2 textureCoords = (textureMatrix * vec4(transformedProjectionCoords, 0.0, 1.0)).xy;
             gl_FragColor = texture2DBlackBorder(sourceTexture, textureCoords);
        }"
        + projectFunctionText()
        + unprojectFunctionText()

    function projectFunctionText()
    {
        switch (lensProjectionType)
        {
            case Utils3D.SphereProjectionTypes.Stereographic:
            {
                return "
                    vec2 project(vec3 coords)
                    {
                         return coords.xy / (1.0 - coords.z);
                    }"
            }

            case Utils3D.SphereProjectionTypes.Equisolid:
            {
                return "
                    vec2 project(vec3 coords)
                    {
                         return coords.xy / sqrt(1.0 - coords.z);
                    }"
            }

            default: // Utils3D.SphereProjectionTypes.Equidistant
            {
                return "
                    vec2 project(vec3 coords)
                    {
                         float theta = acos(clamp(-coords.z, -1.0, 1.0));
                         return coords.xy * (theta / (length(coords.xy) * (pi / 2.0)));
                    }"
            }
        }
    }

    function unprojectFunctionText()
    {
        switch (viewProjectionType)
        {
            case Utils3D.SphereProjectionTypes.Equidistant:
            {
                return "
                    vec3 unproject(vec2 coords)
                    {
                         float r = length(coords);
                         float theta = clamp(r, 0.0, 2.0) * (pi / 2.0);
                         return vec3(coords * sin(theta) / r, -cos(theta));
                    }"
            }

            case Utils3D.SphereProjectionTypes.Equisolid:
            {
                return "
                    vec3 unproject(vec2 coords)
                    {
                         float r2 = dot(coords, coords);
                         return vec3(coords * sqrt(2.0 - r2), r2 - 1.0);
                    }"
            }

            default: // Utils3D.SphereProjectionTypes.Stereographic
            {
                return "
                    vec3 unproject(vec2 coords)
                    {
                         float r2 = dot(coords, coords);
                         return vec3(coords * 2.0, r2 - 1.0) / (r2 + 1.0);
                    }"
            }
        }
    }
}
