#include "getter.h"
#include <QVariant>
#include <QColor>
#include <QMetaType>
#include <QVector4D>
#include <utils/common/warnings.h>

QVariant QnColorPropertyGetter::operator()(const QObject *object) const {
    QVariant result = QnPropertyGetter::operator()(object);
    if(result.userType() != QMetaType::QColor) {
        qnWarning("Invalid property type - expected '%1', got '%2'", QMetaType::typeName(QMetaType::QColor), result.userType());
        return QVariant(QMetaType::QVector4D, static_cast<const void *>(NULL));
    }

    const QColor &color = *static_cast<const QColor *>(result.constData());
    return QVector4D(
        color.red()   / 255.0,
        color.green() / 255.0,
        color.blue()  / 255.0,
        color.alpha() / 255.0
    );
}
