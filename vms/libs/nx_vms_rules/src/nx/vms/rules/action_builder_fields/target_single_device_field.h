// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

/** Displayed as a device selection button and a "Use source" checkbox. */
class NX_VMS_RULES_API TargetSingleDeviceField: public ActionBuilderField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.device")

    Q_PROPERTY(nx::Uuid id READ id WRITE setId NOTIFY idChanged)
    Q_PROPERTY(bool useSource READ useSource WRITE setUseSource NOTIFY useSourceChanged)

public:
    TargetSingleDeviceField() = default;

    nx::Uuid id() const;
    void setId(nx::Uuid id);
    bool useSource() const;
    void setUseSource(bool value);

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

signals:
    void idChanged();
    void useSourceChanged();

private:
    nx::Uuid m_id;
    bool m_useSource{false};
};

} // namespace nx::vms::rules
