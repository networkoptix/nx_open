// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API ResourceGroup
{
    QnUuid id;

    /**%apidoc
     * %example All Devices
     */
    QString name;

    /**%apidoc
     * %example All Devices in this VMS Site.
     */
    QString description;

    /**%apidoc Whether this group comes with the Site and can not be removed. */
    bool isPredefined = false;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }
};
#define ResourceGroup_Fields \
    (id) \
    (name) \
    (description) \
    (isPredefined)
QN_FUSION_DECLARE_FUNCTIONS(ResourceGroup, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ResourceGroup, ResourceGroup_Fields)

} // namespace nx::vms::api
