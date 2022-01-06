// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "converter.h"

#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <nx/utils/log/log.h>

QVariant AbstractConverter::convertSourceToTarget(const QVariant &source) const {
    if (source.metaType() != sourceType())
    {
        NX_ASSERT(false,
            "Source of invalid type: expected '%1', got '%2'.",
            sourceType().name(), source.metaType().name());

        return QVariant(targetType());
    }

    return doConvertSourceToTarget(source);
}

QVariant AbstractConverter::convertTargetToSource(const QVariant &target) const {
    if (target.metaType() != targetType())
    {
        NX_ASSERT(false,
            "Target of invalid type: expected '%1', got '%2'.",
            targetType().name(),
            target.metaType().name());

        return QVariant(sourceType());
    }

    return doConvertTargetToSource(target);
}

