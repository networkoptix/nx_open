#ifndef QN_SHADER_SOURCE_H
#define QN_SHADER_SOURCE_H

#ifndef Q_MOC_RUN
#include <boost/preprocessor/stringize.hpp>
#endif

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

#include <QtCore/QByteArray>
#include <QtGui/QSurfaceFormat>

inline QByteArray qnProcessShaderSource(const char* parenthesized)
{
    QByteArray result;

    // Remove leading '('.
    if (NX_ASSERT(parenthesized[0] == '('))
        result.append(parenthesized + 1); 

    // Remove trailing ')'.
    if (NX_ASSERT(result.back() == ')'))
        result.chop(1);

    const auto profile = QSurfaceFormat::defaultFormat().profile();

    if (profile != QSurfaceFormat::CoreProfile)
        return result;

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

#define QN_SHADER_SOURCE(...) qnProcessShaderSource(BOOST_PP_STRINGIZE((__VA_ARGS__)))

#endif // QN_SHADER_SOURCE_H
