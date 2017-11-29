#pragma once

#include <memory>
#include <atomic>

#include <QtCore/QString>

#include <detection_plugin_interface/detection_plugin_interface.h>

#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>

namespace nx {
namespace analytics {

class DetectionPluginFactory:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT;
public:
    DetectionPluginFactory(QObject* parent);
    std::unique_ptr<AbstractDetectionPlugin> createDetectionPlugin(const QString& id);

private:
    CreateDetectionPluginProcedure extractFactoryFunction(const QString& pathToPlugin);

private:
    mutable QnMutex m_mutex;
    CreateDetectionPluginProcedure m_factoryFunction = nullptr;
    std::atomic<int> m_instanceCount;
};

} // namespace analytics
} // namespace nx
