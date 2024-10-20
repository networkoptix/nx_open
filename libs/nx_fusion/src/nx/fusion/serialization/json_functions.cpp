// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json_functions.h"

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QVarLengthArray>

#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/fusion/fusion/fusion_adaptors.h>

namespace Qt {

NX_REFLECTION_INSTRUMENT_ENUM(BrushStyle,
    NoBrush,
    SolidPattern,
    Dense1Pattern,
    Dense2Pattern,
    Dense3Pattern,
    Dense4Pattern,
    Dense5Pattern,
    Dense6Pattern,
    Dense7Pattern,
    HorPattern,
    VerPattern,
    CrossPattern,
    BDiagPattern,
    FDiagPattern,
    DiagCrossPattern,
    LinearGradientPattern,
    RadialGradientPattern,
    ConicalGradientPattern,
    TexturePattern
)

} // namespace Qt

QN_FUSION_DEFINE_FUNCTIONS(QnLatin1Array, (json_lexical))
QN_FUSION_DEFINE_FUNCTIONS(QBitArray, (json_lexical))
QN_FUSION_DEFINE_FUNCTIONS(QUrl, (json_lexical))
QN_FUSION_DEFINE_FUNCTIONS(QDateTime, (json_lexical))

QN_FUSION_DEFINE_FUNCTIONS(QSize, (json))
QN_FUSION_DEFINE_FUNCTIONS(QSizeF, (json))
QN_FUSION_DEFINE_FUNCTIONS(QRect, (json))
QN_FUSION_DEFINE_FUNCTIONS(QRectF, (json))
QN_FUSION_DEFINE_FUNCTIONS(QPoint, (json))
QN_FUSION_DEFINE_FUNCTIONS(QPointF, (json))

void serialize(QnJsonContext *ctx, const QByteArray &value, QJsonValue *target) {
    ::serialize(ctx, QString::fromLatin1(value.toBase64()), target); /* Note the direct call instead of invocation through QJson. */
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QByteArray *target) {
    QString string;
    if(!::deserialize(ctx, value, &string)) /* Note the direct call instead of invocation through QJson. */
        return false;

    *target = QByteArray::fromBase64(string.toLatin1());
    return true;
}

void serialize(QnJsonContext* ctx, const nx::Buffer& value, QJsonValue* target) {
    ::serialize(ctx, value.toRawByteArray(), target);
}

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, nx::Buffer* target) {
    QByteArray buf;
    if (!::deserialize(ctx, value, &buf))
        return false;

    *target = nx::Buffer(buf);
    return true;
}

void serialize(QnJsonContext *, const nx::Uuid &value, QJsonValue *target) {
    *target = QnLexical::serialized(value);
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, nx::Uuid *target) {
    /* Support JSON null for nx::Uuid, even though we don't generate it on
     * serialization. */
    if(value.type() == QJsonValue::Null) {
        *target = nx::Uuid();
        return true;
    }

    QString jsonString;
    if(!::deserialize(ctx, value, &jsonString)) /* Note the direct call instead of invocation through QJson. */
        return false;

    return QnLexical::deserialize(jsonString, target);
}

#define TEST_VALUE(TYPE)                                            \
void testValue(const TYPE &value) {                                 \
    QByteArray json;                                                \
    QJson::serialize(value, &json);                                 \
    QString result = QString::fromUtf8(json);                       \
    TYPE newValue;                                                  \
    QJson::deserialize(result.toUtf8(), &newValue);                 \
    NX_ASSERT(value == newValue);                                    \
}

TEST_VALUE(QSize)
TEST_VALUE(QSizeF)
TEST_VALUE(QRect)
TEST_VALUE(QRectF)
TEST_VALUE(QPoint)
TEST_VALUE(QPointF)
TEST_VALUE(nx::Uuid)

void qnJsonFunctionsUnitTest() {
    testValue(QSize(15, 27));
    testValue(QSizeF(2.0, 0.6666));
    testValue(QRect(12, 24, 66, 52));
    testValue(QRectF(1.5, 0.6543, 5.66, 234.2342e+18));
    testValue(QPoint(15, 27));
    testValue(QPointF(4.6, 0.1234));
    testValue(nx::Uuid::createUuid());
}
