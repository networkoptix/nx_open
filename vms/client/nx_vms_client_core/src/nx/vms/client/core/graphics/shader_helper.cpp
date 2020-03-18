#include "shader_helper.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/graphics/shader_source.h>

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
    qmlRegisterSingletonType<ShaderHelper>("nx.client.core.graphics", 1, 0, "ShaderHelper",
        shaderHelperProvider);
}

} // namespace nx::vms::client::core::graphics
