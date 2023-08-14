// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class SystemContext;

QString getBookmarkCreatorName(const QnUuid& creatorId, SystemContext* context);

} // namespace nx::vms::client::desktop
