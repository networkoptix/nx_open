#include "json_functions.h"

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QVarLengthArray>

QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QColor)

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

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QVector2D, 
    ((&QVector2D::x, &QVector2D::setX, "x"))
    ((&QVector2D::y, &QVector2D::setY, "y"))
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QVector3D, 
    ((&QVector3D::x, &QVector3D::setX, "x"))
    ((&QVector3D::y, &QVector3D::setY, "y"))
    ((&QVector3D::z, &QVector3D::setZ, "z"))
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QVector4D, 
    ((&QVector4D::x, &QVector4D::setX, "x"))
    ((&QVector4D::y, &QVector4D::setY, "y"))
    ((&QVector4D::z, &QVector4D::setZ, "z"))
    ((&QVector4D::w, &QVector4D::setW, "w"))
)

void serialize(QnJsonContext *ctx, const QRegion &value, QJsonValue *target) {
    QJson::serialize(ctx, value.rects(), target);
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QRegion *target) {
    if(value.type() == QJsonValue::Null) {
        *target = QRegion();
        return true;
    }

    QVarLengthArray<QRect, 32> rects;
    if(!QJson::deserialize(ctx, value, &rects))
        return false;

    target->setRects(rects.data(), rects.size());
    return true;
}

void serialize(QnJsonContext *, const QUuid &value, QJsonValue *target) {
    *target = QnLexical::serialized(value);
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QUuid *target) {
    /* Support JSON null for QUuid, even though we don't generate it on
     * serialization. */
    if(value.type() == QJsonValue::Null) {
        *target = QUuid();
        return true;
    }

    QString jsonString;
    if(!::deserialize(ctx, value, &jsonString)) /* Note the direct call instead of invocation through QJson. */
        return false;

    return QnLexical::deserialize(jsonString, target);
}

void serialize(QnJsonContext *, const QFont &, QJsonValue *) {
    assert(false); /* Won't need for now. */ // TODO: #Elric
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QFont *target) {
    if(value.type() == QJsonValue::String) {
        *target = QFont(value.toString());
        return true;
    } else if(value.type() == QJsonValue::Object) {
        QJsonObject map = value.toObject();
        
        QString family;
        int pointSize = -1;
        if(
            !QJson::deserialize(ctx, map, lit("family"), &family) ||
            !QJson::deserialize(ctx, map, lit("pointSize"), &pointSize, true)
        ) {
            return false;
        }

        *target = QFont(family, pointSize);
        return true;
    } else {
        return false;
    }
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

void testRegion(int len) {
    QRegion region;
    QRect* rects = new QRect[len];
    for (int i = 0; i < len; i++) {
        QRect rect(0, i, i, 1);
        rects[i] = rect;
    }
    region.setRects(rects, len);
    delete[] rects;

    QByteArray json;
    QJson::serialize(region, &json);
    QRegion newValue;
    QJson::deserialize(json, &newValue);
    Q_ASSERT(region == newValue);
}

void qnJsonFunctionsUnitTest() {
    testValue(QSize(15, 27));
    testValue(QSizeF(2.0, 0.6666));
    testValue(QRect(12, 24, 66, 52));
    testValue(QRectF(1.5, 0.6543, 5.66, 234.2342e+18));
    testValue(QPoint(15, 27));
    testValue(QPointF(4.6, 0.1234));
    testValue(QColor(Qt::gray));
    testValue(QUuid::createUuid());
    testRegion(332);
}
