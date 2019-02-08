#pragma once

#include "data.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API IdData: Data
{
    IdData() = default;
    IdData(const QnUuid& id): id(id) {}

    /**
     * Subclasses can (re)define this method when needed. It is called by name from templates
     * via sfinae, if this method is defined, hence it is non-virtual, and any serializable
     * class which is not inherited from IdData can also define such method.
     * Used for merging with the existing object in POST requests.
     * @return Value of a field holding object guid.
     */
    QnUuid getIdForMerging() const { return id; }

    /**
     * Subclasses can (re)define this method when needed. It is called by name from templates
     * via sfinae, if this method is defined, hence it is non-virtual, and any serializable
     * class which is not inherited from IdData can also define such method together with
     * defining getIdForMerging() (otherwise, this method will not be called).
     * Used for generating ommitted id in POST requests which create new objects. Can set id to
     * a null guid if generating is not possible.
     */
    void fillId() { id = QnUuid::createUuid(); }

    QnUuid id;
};
#define IdData_Fields (id)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::IdData)
Q_DECLARE_METATYPE(nx::vms::api::IdDataList)
