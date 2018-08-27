#pragma once

#include <QtCore/QString>
#include <QtCore/QHash>

class QJsonValue;
class QnJsonContext;

/**
 * @param TYPE Type to declare json (de)serialization functions for.
 * @param PREFIX Optional function declaration prefix, e.g. <code>inline</code>.
 * NOTE: This macro generates function declarations only. Definitions still have to be supplied.
 */
#define QN_FUSION_DECLARE_FUNCTIONS_json(TYPE, ... /* PREFIX */) \
    __VA_ARGS__ void serialize(QnJsonContext* ctx, const TYPE& value, QJsonValue* target); \
    __VA_ARGS__ bool deserialize(QnJsonContext* ctx, const QJsonValue& value, TYPE* target);

/**
 * Defines deprecated aliases for currently used JSON fields, used for backwards compatibility:
 * this map is searched on deserialization when a particular field is not found in JSON.
 * Structures may provide a method getDeprecatedFieldNames() defined like:
 * <pre><code>
 * static DeprecatedFieldNames* getDeprecatedFieldNames()
 * {
 *     static DeprecatedFieldNames kDeprecatedFieldNames{
 *         {QStringLiteral("currentFieldName"),
 *             QStringLiteral("deprecatedFieldName")}, //< up to v2.6
 *     };
 *     return &kDeprecatedFieldNames;
 * }
 * </code></pre>
 * It is called by name from templates via sfinae, if this method is defined. Null result is
 * equivalent to the absence of the method.
 */
typedef const QHash<QString /*current*/, QString /*deprecated*/> DeprecatedFieldNames;

namespace QJson {

template<class T>
QByteArray serialized(const T& value);

template<class T>
T deserialized(const QByteArray& value, const T& defaultValue = T(), bool* success = nullptr);

} // namespace QJson
