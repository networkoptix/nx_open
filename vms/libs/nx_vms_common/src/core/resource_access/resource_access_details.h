// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::core::access {

using ResourceAccessDetails = QHash<
    nx::Uuid /** Id of the subject the access is gained in. */,
    QSet<QnResourcePtr> /** Indirect providers, the resource itself if it's accessed directly. */>;

/** Merges two resource access detail sets. */
NX_VMS_COMMON_API ResourceAccessDetails& operator+=(
    ResourceAccessDetails& destination, const ResourceAccessDetails& source);

// GoogleTest printer.
NX_VMS_COMMON_API void PrintTo(const ResourceAccessDetails& details, std::ostream* os);

} // namespace nx::core::access
