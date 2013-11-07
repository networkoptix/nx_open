#include "json_utils.h"

#include <utils/common/color.h>
#include <utils/common/json5.h>

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

QN_DEFINE_CLASS_SERIALIZATION_FUNCTION(QSize, (width)(height))
QN_DEFINE_CLASS_DESERIALIZATION_FUNCTION(QSize, (Width)(Height), int)
QN_DEFINE_CLASS_SERIALIZATION_FUNCTION(QSizeF, (width)(height))
QN_DEFINE_CLASS_DESERIALIZATION_FUNCTION(QSizeF, (Width)(Height), qreal)

QN_DEFINE_CLASS_SERIALIZATION_FUNCTION(QRect, (left)(top)(width)(height))
QN_DEFINE_CLASS_DESERIALIZATION_FUNCTION(QRect, (Left)(Top)(Width)(Height), int)
QN_DEFINE_CLASS_SERIALIZATION_FUNCTION(QRectF, (left)(top)(width)(height))
QN_DEFINE_CLASS_DESERIALIZATION_FUNCTION(QRectF, (Left)(Top)(Width)(Height), qreal)

QN_DEFINE_CLASS_SERIALIZATION_FUNCTION(QPoint, (x)(y))
QN_DEFINE_CLASS_DESERIALIZATION_FUNCTION(QPoint, (X)(Y), int)
QN_DEFINE_CLASS_SERIALIZATION_FUNCTION(QPointF, (x)(y))
QN_DEFINE_CLASS_DESERIALIZATION_FUNCTION(QPointF, (X)(Y), qreal)


void serialize(const QRect &value, QJsonObject &json) {
    json[QLatin1String("x")] = value.x();
    json[QLatin1String("y")] = value.y();
    json[QLatin1String("width")] = value.width();
    json[QLatin1String("height")] = value.height();
}

bool deserialize(const QJsonObject &json, QRect *target) {
    target->setX(qRound(json[QLatin1String("x")].toDouble()));
    target->setY(qRound(json[QLatin1String("y")].toDouble()));
    target->setWidth(qRound(json[QLatin1String("width")].toDouble()));
    target->setHeight(qRound(json[QLatin1String("height")].toDouble()));
    return true;
}

void serialize(const QRegion &value, QJsonObject &json) {
    QJsonArray rectsArray;
    foreach (const QRect &rect, value.rects()) {
        QJsonObject rectObject;
        serialize(rect, rectObject);
        rectsArray.append(rectObject);
    }
    json[QLatin1String("region")] = rectsArray;
}

bool deserialize(const QJsonObject &json, QRegion *target) {
    QJsonArray rectsArray = json[QLatin1String("region")].toArray();

    QRect* rects;
    rects = new QRect[rectsArray.size()];

    for (int index = 0; index < rectsArray.size(); ++index) {
        QJsonObject rectObject = rectsArray[index].toObject();
        deserialize(rectObject, &rects[index]);

    }
    target->setRects(rects, rectsArray.size());
    delete[] rects;

    return true;
}

void serialize(const QUuid &value, QVariant *target) {
    *target = value.toString();
}

bool deserialize(const QVariant &value, QUuid *target) {
    /* Support JSON null for QUuid, even though we don't generate it on
     * serialization. */
    if(value.type() == QVariant::Invalid) {
        *target = QUuid();
        return true;
    }

    if(value.type() != QVariant::String)
        return false;

    QString string = value.toString();
    QUuid result(string);
    if(result.isNull() && string != QLatin1String("00000000-0000-0000-0000-000000000000") && string != QLatin1String("{00000000-0000-0000-0000-000000000000}"))
        return false;

    *target = result;
    return true;
}

void serialize(const QColor &value, QVariant *target) {
    *target = value.name();
}

bool deserialize(const QVariant &value, QColor *target) {
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

void jsonUtilsUnitTest() {
    testValue(QSize(15, 27));
    testValue(QSizeF(2.0, 0.6666));
    testValue(QRect(12, 24, 66, 52));
    testValue(QRectF(1.5, 0.6543, 5.66, 234.2342e+18));
    testValue(QPoint(15, 27));
    testValue(QPointF(4.6, 0.1234));
    testValue(QColor(Qt::gray));
    testValue(QUuid::createUuid());
}
