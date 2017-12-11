#include "detection_plugin_factory.h"
#include "test_detection_plugin.h"

#define NX_OUTPUT_PREFIX "[Detection plugin factory] ";
#include "config.h"

#include <QtCore/QLibrary>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace analytics {

DetectionPluginFactory::DetectionPluginFactory(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent),
    m_instanceCount(0)
{
}

std::unique_ptr<AbstractDetectionPlugin> DetectionPluginFactory::createDetectionPlugin(const QString& id)
{
    if (ini().singleInstance && m_instanceCount > 0)
        return nullptr;

    if (ini().useTestImplementation)
        return std::unique_ptr<AbstractDetectionPlugin>(new TestDetectionPlugin());

    {
        QnMutexLocker lock(&m_mutex);
        if (!m_factoryFunction)
            m_factoryFunction = extractFactoryFunction(
                QString::fromStdString(std::string(ini().pathToPlugin)));
    }

    if (!m_factoryFunction)
        return nullptr;

    auto instance = m_factoryFunction();
    if (!instance)
        return nullptr;

    AbstractDetectionPlugin::Params params;
    std::string idStr = id.toStdString();

    params.id = idStr.c_str();
    params.modelFile = ini().modelFile;
    params.deployFile = ini().deployFile;
    params.cacheFile = ini().cacheFile;

    instance->setParams(params);

    ++m_instanceCount;
    return std::unique_ptr<AbstractDetectionPlugin>(instance);
}

CreateDetectionPluginProcedure DetectionPluginFactory::extractFactoryFunction(
    const QString& pathToPlugin)
{
    QLibrary lib(pathToPlugin);
    if (!lib.load())
        return nullptr;

    auto factoryFunction =
        (CreateDetectionPluginProcedure)
        lib.resolve("createDetectionPluginInstance");

    if (!factoryFunction)
    {
        lib.unload();
        return nullptr;
    }

    return factoryFunction;
}

} // namespace analytics
} // namespace nx
