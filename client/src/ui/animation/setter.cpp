#include "setter.h"
#include <QVariant>
#include <QVector4D>
#include <QMetaType>
#include <QColor>
#include <utils/common/warnings.h>

void QnColorPropertySetter::operator()(QObject *object, const QVariant &value) const {
    if(value.userType() != QMetaType::QVector4D) {
        qnWarning("Invalid property type - expected '%1', got '%2'", QMetaType::typeName(QMetaType::QVector4D), value.userType());
        return;
    }

    const QVector4D &vector = *static_cast<const QVector4D *>(value.constData());

    QnPropertySetter::operator()(
        object, 
        QColor(
            qBound(0, static_cast<int>(vector.x() * 255 + 0.5), 255),
            qBound(0, static_cast<int>(vector.y() * 255 + 0.5), 255),
            qBound(0, static_cast<int>(vector.z() * 255 + 0.5), 255),
            qBound(0, static_cast<int>(vector.w() * 255 + 0.5), 255)
        )
    );
}