#pragma once

#include <core/resource/resource.h>

namespace nx::vms::common {

class AnalyticsPluginResource: public QnResource
{
    using base_type = QnResource;

public:
    AnalyticsPluginResource(QnCommonModule* commonModule = nullptr);
    virtual ~AnalyticsPluginResource() override;

private:
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourceList)
