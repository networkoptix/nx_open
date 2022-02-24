// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common::update {

struct NX_VMS_COMMON_API PersistentUpdateStorage
{
    QList<QnUuid> servers;
    bool autoSelection = false;

    PersistentUpdateStorage(const QList<QnUuid>& servers, bool autoSelection):
        servers(servers),
        autoSelection(autoSelection)
    {}

    PersistentUpdateStorage() = default;

    bool operator==(const PersistentUpdateStorage& other) const = default;
};

#define PersistentUpdateStorage_Fields (servers)(autoSelection)
QN_FUSION_DECLARE_FUNCTIONS(PersistentUpdateStorage, (ubjson)(json), NX_VMS_COMMON_API)

} // namespace nx::vms::common::update

Q_DECLARE_METATYPE(nx::vms::common::update::PersistentUpdateStorage);
