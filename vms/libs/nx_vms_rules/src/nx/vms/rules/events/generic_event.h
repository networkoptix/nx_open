// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API GenericEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.generic")

    FIELD(QString, source, setSource)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)

public:
    GenericEvent() = default;

    GenericEvent(const QString &source, const QString &caption, const QString &description):
        m_source(source),
        m_caption(caption),
        m_description(description)
    {
    }

    static FilterManifest filterManifest();
};

} // namespace nx::vms::rules
