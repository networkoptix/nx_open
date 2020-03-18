#include "color_line_shader_program.h"

#include <ui/graphics/opengl/gl_context_data.h>

#include <nx/vms/client/core/graphics/shader_source.h>


QnColorLineGLShaderProgram::QnColorLineGLShaderProgram(QObject *parent)
    : base_type(parent)
{
}

bool QnColorLineGLShaderProgram::compile()
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
        uniform mat4 uModelViewProjectionMatrix;
        attribute vec4 vData;

        varying float lineCoord;

        void main()
        {
            lineCoord = vData.z;
            gl_Position = uModelViewProjectionMatrix * vec4(vData.xy, 0.0, 1.0);
        }
    ));

    QByteArray shader(QN_SHADER_SOURCE(
        uniform vec4 uColor;
        uniform float feather;

        varying float lineCoord;

        void main()
        {
            float limit = 0.5 - feather;
            float delta = abs(0.5 - lineCoord);
            if (delta > limit)
                gl_FragColor = vec4(uColor.rgb, uColor.a * (1.0 - (delta - limit) / feather));
            else
                gl_FragColor = uColor;
        }
    ));

#ifdef QT_OPENGL_ES_2
    shader = QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);

    return link();
};


namespace {

// Based on https://github.com/vicrucann/shader-3dcurve/blob/master/src/Shaders/polyline.geom
void genSegment(QVector2D p0,
    QVector2D p1,
    QVector2D p2,
    QVector2D p3,
    float thickness,
    float miterLimit,
    std::vector<GLdouble>& vertices)
{
    // Determine the direction of each of the 3 segments (previous, current, next).
    QVector2D v0 = QVector2D(p1 - p0).normalized();
    QVector2D v1 = QVector2D(p2 - p1).normalized();
    QVector2D v2 = QVector2D(p3 - p2).normalized();

    // Determine the normal of each of the 3 segments (previous, current, next).
    QVector2D n0 = QVector2D(-v0.y(), v0.x());
    QVector2D n1 = QVector2D(-v1.y(), v1.x());
    QVector2D n2 = QVector2D(-v2.y(), v2.x());

    // Determine miter lines by averaging the normals of the 2 segments.
    QVector2D miter_a = QVector2D(n0 + n1).normalized(); //< miter at start of current segment
    QVector2D miter_b = QVector2D(n1 + n2).normalized(); //< miter at end of current segment

    // Determine the length of the miter by projecting it onto normal and then inverse it.
    float an1 = QVector2D::dotProduct(miter_a, n1);
    float bn1 = QVector2D::dotProduct(miter_b, n2);
    if (an1 == 0)
        an1 = 1;
    if (bn1 == 0)
        bn1 = 1;
    float length_a = thickness / an1;
    float length_b = thickness / bn1;

    // Prevent excessively long miters at sharp corners.
    if (QVector2D::dotProduct(v0, v1) < -miterLimit)
    {
        miter_a = n1;
        length_a = thickness;

        // Close the gap.
        if (QVector2D::dotProduct(v0, n1) > 0)
        {
            auto pos = p1 + thickness * n0;
            vertices.push_back(pos.x());
            vertices.push_back(pos.y());
            vertices.push_back(0.0);

            pos = p1 + thickness * n1;
            vertices.push_back(pos.x());
            vertices.push_back(pos.y());
            vertices.push_back(0.0);

            pos = p1;
            vertices.push_back(pos.x());
            vertices.push_back(pos.y());
            vertices.push_back(0.5);
        }
        else
        {
            auto pos = p1 - thickness * n1;
            vertices.push_back(pos.x());
            vertices.push_back(pos.y());
            vertices.push_back(1.0);

            pos = p1 - thickness * n0;
            vertices.push_back(pos.x());
            vertices.push_back(pos.y());
            vertices.push_back(1.0);

            pos = p1;
            vertices.push_back(pos.x());
            vertices.push_back(pos.y());
            vertices.push_back(0.5);
        }
    }
    if (QVector2D::dotProduct(v1, v2) < -miterLimit)
    {
        miter_b = n1;
        length_b = thickness;
    }

    // Generate the triangles.

    // Vertex 0
    auto pos = p1 + length_a * miter_a;
    vertices.push_back(pos.x());
    vertices.push_back(pos.y());
    vertices.push_back(0.0);

    // Vertex 1
    auto pos1 = p1 - length_a * miter_a;
    vertices.push_back(pos1.x());
    vertices.push_back(pos1.y());
    vertices.push_back(1.0);

    // Vertex 2
    auto pos2 = p2 + length_b * miter_b;
    vertices.push_back(pos2.x());
    vertices.push_back(pos2.y());
    vertices.push_back(0.0);

    // Vertex 1
    vertices.push_back(pos1.x());
    vertices.push_back(pos1.y());
    vertices.push_back(1.0);

    // Vertex 2
    vertices.push_back(pos2.x());
    vertices.push_back(pos2.y());
    vertices.push_back(0.0);

    // Vertex 3
    pos = p2 - length_b * miter_b;
    vertices.push_back(pos.x());
    vertices.push_back(pos.y());
    vertices.push_back(1.0);
}

} // namespace

std::vector<GLdouble> QnColorLineGLShaderProgram::genVericesFromLineStrip(
    const QVector<QPointF>& points,
    bool closed,
    qreal lineWidth)
{
    std::vector<GLdouble> triangles;
    if (points.size() < 2)
        return triangles;

    auto accessor = [&points, closed](int i)
    {
        if (i == -1)
            return closed ? points.last() : points.first();
        if (i == points.size())
            return closed ? points.first() : points.last();
        if (i == points.size() + 1)
            return points[1];

        return points[i];
    };

    const int numSegments = points.size() - (closed ? 0 : 1);

    for (int i = 0; i < numSegments; i++)
    {
        const QVector2D p0(accessor(i - 1));
        const QVector2D p1(accessor(i));
        const QVector2D p2(accessor(i + 1));
        const QVector2D p3(accessor(i + 2));
        genSegment(p0, p1, p2, p3, lineWidth / 2, 0.0, triangles);
    }

    return triangles;
}
