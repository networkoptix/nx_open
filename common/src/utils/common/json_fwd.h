#ifndef QN_JSON_FWD_H
#define QN_JSON_FWD_H

class QJsonValue;

/**
 * \param TYPE                          Type to declare json (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)         \
__VA_ARGS__ void serialize(const TYPE &value, QJsonValue *target);              \
__VA_ARGS__ bool deserialize(const QJsonValue &value, TYPE *target);

#endif // QN_JSON_FWD_H
