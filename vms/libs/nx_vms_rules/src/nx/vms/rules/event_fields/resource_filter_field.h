// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ResourceFilterField: public EventField
{
    Q_OBJECT

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll)
    Q_PROPERTY(QnUuidList ids READ ids WRITE setIds)

public:
    virtual bool match(const QVariant& value) const override;

    bool acceptAll() const;
    void setAcceptAll(bool anyCamera);

    QnUuidList ids() const;
    void setIds(const QnUuidList& ids);

protected:
    // This field type should be used as base class only.
    ResourceFilterField();

private:
    bool m_acceptAll;
    QnUuidList m_ids;
};

} // namespace nx::vms::rules
