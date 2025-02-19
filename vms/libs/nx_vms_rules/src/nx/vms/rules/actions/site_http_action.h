// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>

#include <nx/network/http/http_types.h>

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SiteHttpAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "siteHttp")

    FIELD(std::chrono::microseconds, interval, setInterval)
    FIELD(QString, endpoint, setEndpoint)
    FIELD(QString, content, setContent)
    FIELD(QString, method, setMethod)

public:
    static const ItemDescriptor& manifest();

    QVariantMap details(common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
