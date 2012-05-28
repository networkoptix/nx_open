#include "converter.h"
#include <cassert>
#include <QVariant>
#include <QMetaType>
#include <QVector4D>
#include <QColor>
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

QnColorToVectorConverter::QnColorToVectorConverter():
    AbstractConverter(QMetaType::QColor, QMetaType::QVector4D)
{}

QVariant QnColorToVectorConverter::doConvertSourceToTarget(const QVariant &source) const {
    assert(source.userType() == QMetaType::QColor);

    const QColor &color = *static_cast<const QColor *>(source.constData());
    return QVector4D(
        color.red()   / 255.0,
        color.green() / 255.0,
        color.blue()  / 255.0,
        color.alpha() / 255.0
    );
}

QVariant QnColorToVectorConverter::doConvertTargetToSource(const QVariant &target) const {
    assert(target.userType() == QMetaType::QVector4D);

    const QVector4D &vector = *static_cast<const QVector4D *>(target.constData());
    return QColor(
        qBound(0, static_cast<int>(vector.x() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(vector.y() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(vector.z() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(vector.w() * 255 + 0.5), 255)
    );
}
