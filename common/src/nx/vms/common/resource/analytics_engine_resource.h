#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource.h>

namespace nx::vms::common {

class AnalyticsEngineResource: public QnResource
{
    using base_type = QnResource;

public:
    static QString kSettingsValuesProperty;

    AnalyticsEngineResource(QnCommonModule* commonModule = nullptr);
    virtual ~AnalyticsEngineResource() override;

    QVariantMap settingsValues() const;
    void setSettingsValues(const QVariantMap& values);
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourceList)
