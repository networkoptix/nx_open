// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API PersistentUpdateStorage
{
    QList<nx::Uuid> servers;
    bool autoSelection = false;

    bool operator==(const PersistentUpdateStorage& other) const = default;
    QByteArray toString() const;
};
#define PersistentUpdateStorage_Fields (servers)(autoSelection)
QN_FUSION_DECLARE_FUNCTIONS(PersistentUpdateStorage, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PersistentUpdateStorage, PersistentUpdateStorage_Fields)

} // namespace nx::vms::api
