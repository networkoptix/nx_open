// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::core::access {

using ResourceAccessDetails = QHash<
    QnUuid /** Id of the subject the access is gained in. */,
    QSet<QnResourcePtr> /** Indirect providers, the resource itself if it's accessed directly. */>;

/** Merges two resource access maps. */
NX_VMS_COMMON_API ResourceAccessDetails& operator+=(
    ResourceAccessDetails& destination, const ResourceAccessDetails& source);

} // namespace nx::core::access
