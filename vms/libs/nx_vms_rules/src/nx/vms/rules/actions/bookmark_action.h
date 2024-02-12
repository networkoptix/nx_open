// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API BookmarkAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.bookmark")

    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(std::chrono::microseconds, duration, setDuration)
    FIELD(std::chrono::microseconds, recordBefore, setRecordBefore)
    FIELD(std::chrono::microseconds, recordAfter, setRecordAfter)
    FIELD(QString, tags, setTags)

    FIELD(QString, name, setName)
    FIELD(QStringList, description, setDescription)

public:
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
