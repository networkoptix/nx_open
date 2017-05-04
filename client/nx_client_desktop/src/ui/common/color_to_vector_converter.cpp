#include "color_to_vector_converter.h"

#include <cassert>

#include <QtCore/QMetaType>

void convert(const QVector4D &source, QColor *target) {
    *target = QColor(
        qBound(0, static_cast<int>(source.x() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(source.y() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(source.z() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(source.w() * 255 + 0.5), 255)
    );
}

void convert(const QColor &source, QVector4D *target) {
    *target = QVector4D(
        source.red()   / 255.0,
        source.green() / 255.0,
        source.blue()  / 255.0,
        source.alpha() / 255.0
    );
}

QnColorToVectorConverter::QnColorToVectorConverter():
    AbstractConverter(QMetaType::QColor, QMetaType::QVector4D)
{}

QVariant QnColorToVectorConverter::doConvertSourceToTarget(const QVariant &source) const {
    NX_ASSERT(source.userType() == QMetaType::QColor);

    return convert<QVector4D>(*static_cast<const QColor *>(source.constData()));
}

QVariant QnColorToVectorConverter::doConvertTargetToSource(const QVariant &target) const {
    NX_ASSERT(target.userType() == QMetaType::QVector4D);

    return convert<QColor>(*static_cast<const QVector4D *>(target.constData()));
}
