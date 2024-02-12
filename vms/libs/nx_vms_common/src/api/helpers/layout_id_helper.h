// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx {

/**
 * Finds layouts by so-called FlexibleId which can be id or logical id.
 */
namespace layout_id_helper {

/**
 * @param resourcePool Resources pool where the search will occur.
 * @param flexibleId Id or logical id in the string form.
 * @return Layout, or null if not found.
 */
NX_VMS_COMMON_API QnLayoutResourcePtr findLayoutByFlexibleId(
    const QnResourcePool* resourcePool, const QString& flexibleId);

/**
 * @param resourcePool Resources pool where the search will occur.
 * @param flexibleId Id or logical id in the string form.
 * @return Layout id, or null if not found.
 */
NX_VMS_COMMON_API nx::Uuid flexibleIdToId(const QnResourcePool* resourcePool, const QString& flexibleId);

} // namespace layout_id_helper
} // namespace nx
