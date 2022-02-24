// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "converter.h"

#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <nx/utils/log/log.h>

QVariant AbstractConverter::convertSourceToTarget(const QVariant &source) const {
    if(source.userType() != sourceType()) {
        NX_ASSERT(false,
            "Source of invalid type: expected '%1', got '%2'.",
            QMetaType::typeName(sourceType()), QMetaType::typeName(source.userType()));

        return QVariant(targetType(), static_cast<const void *>(nullptr));
    }

    return doConvertSourceToTarget(source);
}

QVariant AbstractConverter::convertTargetToSource(const QVariant &target) const {
    if(target.userType() != targetType()) {
        NX_ASSERT(false,
            "Target of invalid type: expected '%1', got '%2'.",
            QMetaType::typeName(targetType()),
            QMetaType::typeName(target.userType()));

        return QVariant(sourceType(), static_cast<const void *>(nullptr));
    }

    return doConvertTargetToSource(target);
}

