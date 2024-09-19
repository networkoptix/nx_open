// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/resource_filter_field.h"

namespace nx::vms::rules {

constexpr auto kHasBuzzerValidationPolicy = "hasBuzzer";

/** Displayed as device selection button and "Use source" checkbox. */
class NX_VMS_RULES_API TargetServersField: public ResourceFilterActionField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.servers")

    Q_PROPERTY(bool useSource READ useSource WRITE setUseSource NOTIFY useSourceChanged)

public:
    using ResourceFilterActionField::ResourceFilterActionField;

    bool useSource() const;
    void setUseSource(bool value);

    QVariant build(const AggregatedEventPtr& eventAggregator) const override;
    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void useSourceChanged();

private:
    bool m_useSource = false;
};

} // namespace nx::vms::rules
