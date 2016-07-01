#include "converter.h"

#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <utils/common/warnings.h>

QVariant AbstractConverter::convertSourceToTarget(const QVariant &source) const {
    if(source.userType() != sourceType()) {
        qnWarning("Source of invalid type: expected '%1', got '%2'.", QMetaType::typeName(sourceType()), QMetaType::typeName(source.userType()));

        return QVariant(targetType(), static_cast<const void *>(NULL));
    }

    return doConvertSourceToTarget(source);
}

QVariant AbstractConverter::convertTargetToSource(const QVariant &target) const {
    if(target.userType() != targetType()) {
        qnWarning("Target of invalid type: expected '%1', got '%2'.", QMetaType::typeName(targetType()), QMetaType::typeName(target.userType()));

        return QVariant(sourceType(), static_cast<const void *>(NULL));
    }

    return doConvertTargetToSource(target);
}

