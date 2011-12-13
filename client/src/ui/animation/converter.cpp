#include "converter.h"
#include <QVariant>
#include <QMetaType>
#include <QVector4D>
#include <QColor>

QVariant QnVectorToColorConverter::convertTo(const QVariant &source) const {
    if(source.userType() != QMetaType::QVector4D)
        return source;

    const QVector4D &vector = *static_cast<const QVector4D *>(source.constData());
    return QColor(
        qBound(0, static_cast<int>(vector.x() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(vector.y() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(vector.z() * 255 + 0.5), 255),
        qBound(0, static_cast<int>(vector.w() * 255 + 0.5), 255)
    );
}

QVariant QnVectorToColorConverter::convertFrom(const QVariant &source) const {
    if(source.userType() != QMetaType::QVector4D)
        return source;
    
    const QColor &color = *static_cast<const QColor *>(source.constData());
    return QVector4D(
        color.red()   / 255.0,
        color.green() / 255.0,
        color.blue()  / 255.0,
        color.alpha() / 255.0
    );
}
