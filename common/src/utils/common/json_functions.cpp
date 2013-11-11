#include "json_functions.h"

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QVarLengthArray>

#include <utils/common/color.h>

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QSize, 
    ((&QSize::width, &QSize::setWidth, "width"))
    ((&QSize::height, &QSize::setHeight, "height"))
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QSizeF, 
    ((&QSizeF::width, &QSizeF::setWidth, "width"))
    ((&QSizeF::height, &QSizeF::setHeight, "height"))
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QRect, 
    ((&QRect::left, &QRect::setLeft, "x"))
    ((&QRect::top, &QRect::setTop, "y"))
    ((&QRect::width, &QRect::setWidth, "width"))
    ((&QRect::height, &QRect::setHeight, "height"))
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QRectF, 
    ((&QRectF::left, &QRectF::setLeft, "x"))
    ((&QRectF::top, &QRectF::setTop, "y"))
    ((&QRectF::width, &QRectF::setWidth, "width"))
    ((&QRectF::height, &QRectF::setHeight, "height"))
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QPoint, 
    ((&QPoint::x, &QPoint::setX, "x"))
    ((&QPoint::y, &QPoint::setY, "y"))
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QPointF, 
    ((&QPointF::x, &QPointF::setX, "x"))
    ((&QPointF::y, &QPointF::setY, "y"))
)

void serialize(const QRegion &value, QJsonValue *target) {
    QJson::serialize(value.rects(), target);
}

bool deserialize(const QJsonValue &value, QRegion *target) {
    if(value.type() == QJsonValue::Null) {
        *target = QRegion();
        return true;
    }

    QVarLengthArray<QRect, 32> rects;
    if(!QJson::deserialize(value, &rects))
        return false;

    target->setRects(rects.data(), rects.size());
    return true;
}

void serialize(const QUuid &value, QJsonValue *target) {
    *target = value.toString();
}

bool deserialize(const QJsonValue &value, QUuid *target) {
    /* Support JSON null for QUuid, even though we don't generate it on
     * serialization. */
    if(value.type() == QJsonValue::Null) {
        *target = QUuid();
        return true;
    }

    QString jsonString;
    if(!QJson::deserialize(value, &jsonString))
        return false;

    QUuid result(jsonString);
    if(result.isNull() && jsonString != QLatin1String("00000000-0000-0000-0000-000000000000") && jsonString != QLatin1String("{00000000-0000-0000-0000-000000000000}"))
        return false;

    *target = result;
    return true;
}

void serialize(const QColor &value, QJsonValue *target) {
    *target = value.name();
}

bool deserialize(const QJsonValue &value, QColor *target) {
    if(value.type() != QVariant::String)
        return false;
    *target = parseColor(value);
    return true;
}

#define TEST_VALUE(TYPE)                                            \
void testValue(const TYPE &value) {                                 \
    QByteArray json;                                                \
    QJson::serialize(value, &json);                                 \
    QString result = QString::fromUtf8(json);                       \
    TYPE newValue;                                                  \
    QJson::deserialize(result.toUtf8(), &newValue);                 \
    Q_ASSERT(value == newValue);                                    \
}

TEST_VALUE(QSize)
TEST_VALUE(QSizeF)
TEST_VALUE(QRect)
TEST_VALUE(QRectF)
TEST_VALUE(QPoint)
TEST_VALUE(QPointF)
TEST_VALUE(QColor)
TEST_VALUE(QUuid)

void qnJsonFunctionsUnitTest() {
    testValue(QSize(15, 27));
    testValue(QSizeF(2.0, 0.6666));
    testValue(QRect(12, 24, 66, 52));
    testValue(QRectF(1.5, 0.6543, 5.66, 234.2342e+18));
    testValue(QPoint(15, 27));
    testValue(QPointF(4.6, 0.1234));
    testValue(QColor(Qt::gray));
    testValue(QUuid::createUuid());
}
