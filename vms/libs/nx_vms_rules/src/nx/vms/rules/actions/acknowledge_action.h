// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AcknowledgeAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "acknowledge")
    using base_type = BasicAction;

    FIELD(nx::Uuid, originalId, setOriginalId)
    FIELD(QString, bookmarkId, setBookmarkId)
    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(nx::Uuid, userId, setUserId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)

public:
    static const ItemDescriptor& manifest();

    virtual QVariantMap details(common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
