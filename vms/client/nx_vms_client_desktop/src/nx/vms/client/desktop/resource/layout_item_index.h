// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <nx/utils/uuid.h>

#include "resource_fwd.h"

namespace nx::vms::client::desktop {

/**
 * This class contains all the necessary information to look up a layout item.
 */
class NX_VMS_CLIENT_DESKTOP_API LayoutItemIndex
{
public:
    LayoutItemIndex();
    LayoutItemIndex(const LayoutResourcePtr& layout, const nx::Uuid& uuid);

    const LayoutResourcePtr& layout() const;
    void setLayout(const LayoutResourcePtr& layout);

    const nx::Uuid& uuid() const;
    void setUuid(const nx::Uuid& uuid);

    bool isNull() const;

    /** Debug string representation. */
    QString toString() const;

private:
    LayoutResourcePtr m_layout;
    nx::Uuid m_uuid;
};

using LayoutItemIndexList = QList<LayoutItemIndex>;

} // namespace nx::vms::client::desktop
