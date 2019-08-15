#pragma once

#include <nx/vms/server/analytics/wrappers/types.h>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

namespace nx::vms::server::analytics::wrappers {

class SdkObjectDescription
{
public:
    SdkObjectDescription() = default;

    SdkObjectDescription(
        resource::AnalyticsPluginResourcePtr plugin,
        resource::AnalyticsEngineResourcePtr engine = resource::AnalyticsEngineResourcePtr(),
        QnVirtualCameraResourcePtr device = QnVirtualCameraResourcePtr());

    SdkObjectDescription(QString libraryName);

    resource::AnalyticsPluginResourcePtr plugin() const;
    resource::AnalyticsEngineResourcePtr engine() const;
    QnVirtualCameraResourcePtr device() const;

    SdkObjectType sdkObjectType() const;

    QString descriptionString() const;

    QString baseInputOutputFilename() const;

private:
    const resource::AnalyticsPluginResourcePtr m_plugin;
    const resource::AnalyticsEngineResourcePtr m_engine;
    const QnVirtualCameraResourcePtr m_device;
    const QString m_libraryName;
};

} // namespace nx::vms::server::analytics::wrappers
