#include "shader_source.h"

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

#include <QtCore/QByteArray>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QOpenGLContext>

namespace nx::vms::client::core::graphics {

QByteArray processShaderSource(const char* parenthesized)
{
    QByteArray result;

    // Remove leading '('.
    if (NX_ASSERT(parenthesized[0] == '('))
        result.append(parenthesized + 1); 

    // Remove trailing ')'.
    if (NX_ASSERT(result.back() == ')'))
        result.chop(1);

    return modernizeShaderSource(result);
}

QByteArray modernizeShaderSource(const QByteArray& source)
{
    QByteArray result(source);

    const auto profile = QSurfaceFormat::defaultFormat().profile();

    if (profile != QSurfaceFormat::CoreProfile)
    {
        result.prepend(QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES
                ? "#version 100\n"
                : "#version 120\n");
        return result;
    }

    // Modernize shared source.

    const bool isFragment = result.contains("gl_FragColor");

    if (isFragment)
    {
        result.prepend("layout(location = 0, index = 0) out vec4 FragColor;\n");
        result.replace("gl_FragColor", "FragColor");

        // GL_LUMINANCE_ALPHA -> GL_RG
        result.replace(").q,", ").g,");

        result.replace("texture2D", "texture");
    }

    result.replace("attribute", "in");
    result.replace("varying", isFragment ? "in" : "out");

    result.prepend("#version 330\n");

    return result;
}

} // namespace nx::vms::client::core::graphics
