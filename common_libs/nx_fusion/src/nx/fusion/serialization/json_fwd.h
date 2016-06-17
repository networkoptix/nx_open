#ifndef QN_JSON_FWD_H
#define QN_JSON_FWD_H

class QJsonValue;
class QnJsonContext;

/**
 * \param TYPE                          Type to declare json (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_FUSION_DECLARE_FUNCTIONS_json(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ void serialize(QnJsonContext *ctx, const TYPE &value, QJsonValue *target); \
__VA_ARGS__ bool deserialize(QnJsonContext *ctx, const QJsonValue &value, TYPE *target);

#endif // QN_JSON_FWD_H
