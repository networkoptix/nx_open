// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shader_helper.h"

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

#include <QtCore/QByteArray>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQml/QtQml>

namespace nx::vms::client::core::graphics {

namespace {

static QObject* shaderHelperProvider(QQmlEngine*, QJSEngine*)
{
    return new ShaderHelper();
}

} // namespace

QString ShaderHelper::processSource(const QString& source)
{
    return QString::fromUtf8(modernizeShaderSource(source.toUtf8()));
}

void ShaderHelper::registerQmlType()
{
    qmlRegisterSingletonType<ShaderHelper>("nx.vms.client.core", 1, 0, "ShaderHelper",
        shaderHelperProvider);
}

QByteArray ShaderHelper::modernizeShaderSource(const QByteArray& source)
{
    QByteArray result(source);

    if (source.startsWith("#version"))
        return result;

    const auto profile = QSurfaceFormat::defaultFormat().profile();

    if (profile != QSurfaceFormat::CoreProfile)
    {
        result.prepend(QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES
                ? "#version 100\n"
                : "#version 120\n");
        return result;
    }

    // Modernize shader source.

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

std::tuple<int, int> ShaderHelper::getTexImageFormats()
{
    #if defined(GL_RED) && defined(GL_RG)
        if (QSurfaceFormat::defaultFormat().profile() == QSurfaceFormat::CoreProfile)
            return {GL_RED, GL_RG};
    #endif

    return {GL_LUMINANCE, GL_LUMINANCE_ALPHA};
}

QByteArray preprocessShaderSource(const char* parenthesized)
{
    QByteArray result;

    // Remove leading '('.
    if (NX_ASSERT(parenthesized[0] == '('))
        result.append(parenthesized + 1);

    // Remove trailing ')'.
    if (NX_ASSERT(result.back() == ')'))
        result.chop(1);

    return ShaderHelper::modernizeShaderSource(result);
}

} // namespace nx::vms::client::core::graphics
