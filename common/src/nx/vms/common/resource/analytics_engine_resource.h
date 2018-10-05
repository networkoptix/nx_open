#pragma once

#include <core/resource/resource.h>

namespace nx::vms::common {

class AnalyticsEngineResource: public QnResource
{
    using base_type = QnResource;

public:
    AnalyticsEngineResource(QnCommonModule* commonModule = nullptr);
    virtual ~AnalyticsEngineResource() override;

private:
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourceList)
