// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct NovItemProperties
{
    QString name;
    qint64 timeZoneOffset = 0;
};
using NovItemPropertiesMap = std::unordered_map<QnUuid, NovItemProperties>;
NX_REFLECTION_INSTRUMENT(NovItemProperties, (name)(timeZoneOffset))

/** Metadata file, describing exported nov-file contents. */
struct NovMetadata
{
    static constexpr int kCurrentVersion = 52;

    int version = 0;
    NovItemPropertiesMap itemProperties;
};
NX_REFLECTION_INSTRUMENT(NovMetadata, (version)(itemProperties))

} // namespace nx::vms::client::desktop
