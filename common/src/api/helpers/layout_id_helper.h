#pragma once

#include <core/resource/resource_fwd.h>

namespace nx {

/**
 * Finds layouts by so-called FlexibleId which can be id or logical id.
 */
namespace layout_id_helper {

/**
 * @param resourcePool Resources pool where the search will occur.
 * @return Camera, or null if not found.
 */
QnLayoutResourcePtr findLayoutByFlexibleId(
    QnResourcePool* resourcePool,
    const QString& flexibleId);

/**
 * @param resourcePool Resources pool where the search will occur.
 * @return Layout id, or null if not found.
 */
QnUuid flexibleIdToId(QnResourcePool* resourcePool, const QString& flexibleId);

} // namespace layout_id_helper
} // namespace nx
