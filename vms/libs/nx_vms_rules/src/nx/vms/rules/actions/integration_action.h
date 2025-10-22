// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>

#include "../basic_action.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API IntegrationAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
public:
    IntegrationAction(const QString& type): m_type(type) {};

    QString type() const override { return m_type; }

    static const ItemDescriptor& manifest();

private:
    const QString m_type;
};

} // namespace nx::vms::rules
