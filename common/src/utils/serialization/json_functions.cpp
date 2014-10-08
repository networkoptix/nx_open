#include "json_functions.h"

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QVarLengthArray>

#include <utils/serialization/lexical_functions.h>
#include <utils/fusion/fusion_adaptors.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, BrushStyle, static)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (QnLatin1Array)(QColor)(QUrl),
    (json_lexical)
)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (QSize)(QSizeF)(QRect)(QRectF)(QPoint)(QPointF)(QVector2D)(QVector3D)(QVector4D),
    (json)
)

void serialize(QnJsonContext *ctx, const QByteArray &value, QJsonValue *target) {
    ::serialize(ctx, QString::fromLatin1(value.toBase64()), target); /* Note the direct call instead of invocation through QJson. */
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QByteArray *target) {
    QString string;
    if(!::deserialize(ctx, value, &string)) /* Note the direct call instead of invocation through QJson. */
        return false;

    // TODO: #Elric we don't check for validity, this is not good.
    *target = QByteArray::fromBase64(string.toLatin1());
    return true;
}

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

void serialize(QnJsonContext *ctx, const QBrush &value, QJsonValue *target) {
    if(value.style() == Qt::SolidPattern) {
        QJson::serialize(ctx, value.color(), target);
    } else {
        QJsonObject map;
        QJson::serialize(ctx, value.color(), lit("color"), &map);
        QJson::serialize(ctx, value.style(), lit("style"), &map);
        *target = map;
    }
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QBrush *target) {
    if(value.type() == QJsonValue::String) {
        QColor color;
        if(!QJson::deserialize(ctx, value, &color))
            return false;
        *target = color;
        return true;
    } else if(value.type() == QJsonValue::Object) {
        QJsonObject map = value.toObject();

        QColor color;
        Qt::BrushStyle style = Qt::SolidPattern;
        if(
            !QJson::deserialize(ctx, map, lit("color"), &color) ||
            !QJson::deserialize(ctx, map, lit("style"), &style)
        ) {
            return false;
        }

        *target = QBrush(color, style);
        return true;
    } else {
        return false;
    }
}

void serialize(QnJsonContext *, const QnUuid &value, QJsonValue *target) {
    *target = QnLexical::serialized(value);
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnUuid *target) {
    /* Support JSON null for QnUuid, even though we don't generate it on
     * serialization. */
    if(value.type() == QJsonValue::Null) {
        *target = QnUuid();
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
TEST_VALUE(QnUuid)

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
    testValue(QnUuid::createUuid());
    testRegion(332);
}
