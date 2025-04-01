// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>
#include <string>

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API IntegrationAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "integration")

    FIELD(QString, integrationAction, setIntegrationAction)
    FIELD(QJsonObject, integrationActionParameters, setIntegrationActionParameters)

public:
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
