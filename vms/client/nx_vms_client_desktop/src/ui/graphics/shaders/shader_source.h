#ifndef QN_SHADER_SOURCE_H
#define QN_SHADER_SOURCE_H

#ifndef Q_MOC_RUN
#include <boost/preprocessor/stringize.hpp>
#endif

#include <nx/utils/app_info.h>

#include <QtCore/QByteArray>
#include <QtGui/QSurfaceFormat>
#include <QDebug>

inline QByteArray qnUnparenthesize(const char *parenthesized) {
    QByteArray result;

    result.append(parenthesized + 1);
    result.chop(1);

    const auto profile = QSurfaceFormat::defaultFormat().profile();

    if (profile == QSurfaceFormat::CoreProfile && nx::utils::AppInfo::isMacOsX())
    {
        // Modernize shared source.

        const bool isFragment = result.contains("gl_FragColor");

        if (isFragment)
        {
            result.prepend("layout(location = 0, index = 0) out vec4 FragColor;\n");
            result.replace("gl_FragColor", "FragColor");

            // GL_LUMINANCE_ALPHA -> GL_RG
            result.replace(").a,", ").g,");

            // GL_LUMINANCE -> GL_RED
            result.replace(").p,", ").r,");
            result.replace(").p;", ").r;");
            result.replace(").p)", ").r)");
            result.replace(").p-", ").r-");

            result.replace("texture2D", "texture");
        }

        result.replace("attribute", "in");
        result.replace("varying", isFragment ? "in" : "out");

        result.prepend("#version 330\n");
    }

    return result;
}

#define QN_SHADER_SOURCE(...) qnUnparenthesize(BOOST_PP_STRINGIZE((__VA_ARGS__)))

#endif // QN_SHADER_SOURCE_H
