// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/type_traits.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API IdData
{
    IdData() = default;
    IdData(const QnUuid& id): id(id) {}
    IdData(const QString& id): id(id) {}

    bool operator==(const IdData& other) const = default;

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
     * Used for generating omitted id in POST requests which create new objects. Can set id to
     * a null guid if generating is not possible.
     */
    void fillId() { id = QnUuid::createUuid(); }

    QnUuid getId() const { return id; }
    void setId(QnUuid value) { id = std::move(value); }
    static_assert(isFlexibleIdModelV<IdData>);

    QnUuid id;
};
#define IdData_Fields (id)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(IdData)
NX_REFLECTION_INSTRUMENT(IdData, IdData_Fields)

} // namespace nx::vms::api
