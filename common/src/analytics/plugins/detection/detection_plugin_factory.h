#pragma once

#include <memory>

#include <QtCore/QString>

#include <detection_plugin_interface/detection_plugin_interface.h>

#include <common/common_module_aware.h>

namespace nx {
namespace analytics {

class DetectionPluginFactory: public QnCommonModuleAware
{
public:
    DetectionPluginFactory(QnCommonModule* commonModule);
    std::unique_ptr<AbstractDetectionPlugin> createDetectionPlugin(const QString& id);

private:
    CreateDetectionPluginProcedure extractFactoryFunction(const QString& pathToPlugin);

private:
    CreateDetectionPluginProcedure m_factoryFunction = nullptr;
    int m_instanceCount = 0;
};

} // namespace analytics
} // namespace nx
